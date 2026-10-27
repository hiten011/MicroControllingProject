/*
 * sim_main.c - runs the student's firmware against the simulated Kobuki.
 *
 *   kobuki_sim --layout A --deposit 2 --heading 135
 *   kobuki_sim --trials A2,B1              back-to-back, one program run
 *   kobuki_sim --b1-at 6 / --lift-at 6     safety checks
 *   kobuki_sim --suite                     every layout x deposit x heading
 *
 * The firmware's main() is renamed firmware_main() at compile time and
 * called once. From then on the simulator only gets control when the
 * firmware touches USART3 / ADC1; each time it waits for the next Kobuki
 * packet, sim_feed_needed() advances the world by 20 ms.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sim.h"

int firmware_main(void);

Sim_Config cfg;

/* ---- Trace ----------------------------------------------------------- */

typedef struct {
    uint32_t t;
    uint8_t  trial;
    float    x, y, th;
    uint16_t range, adc, gpo;
    uint8_t  bump, cliff, drop, btn, lifted;
    int16_t  cmd_v, cmd_r;
    float    vl, vr;
    int16_t  gyro;
    uint16_t enc;
} Frame;

static Frame *frames;
static size_t n_frames, cap_frames;

typedef struct { uint32_t t; int trial; char kind[12]; char msg[160]; } Event;
static Event *events;
static size_t n_events, cap_events;

static char warn_keys[64][16];
static char warn_msgs[64][200];
static int  n_warn;

static char driver_summary[200];
static int  driver_problems;

/* ---- Trials ------------------------------------------------------------ */

typedef struct {
    Trial_Spec spec;
    double t_b0;                /* s, sim time of the B0 press               */
    int    side_sec;            /* 2 or 4: first side section entered        */
    double t_sec[5];            /* s after B0, <0 = never                    */
    int    bollard_hits;
    char   hit_names[48];
    int    marker_hit;
    int    fell;
    double max_wheel;
    int    speed_viol;
    char   outcome[96];
    double t_end;               /* s after B0                                */
    double gap, align, margin;
    int    in_storage;
    int    led1, led2;
    double end_x, end_y, end_th;
    int    safety;              /* 0 none, 1 B1, 2 lift                      */
    double safety_cmd_ms, safety_stop_ms;
    int    safety_restart;
    int    n_checks, n_pass;
} Trial_Result;

static Trial_Result res[MAX_TRIALS];
static int          ti;                 /* current trial                  */
static int          trials_done;

static uint32_t t_place;                /* robot put down (ms)            */
static uint32_t lifted_until;           /* relocation lift                */
static uint32_t t_b0_ms;
static uint32_t t_safety, t_safety_end; /* safety event window            */
static uint32_t still_since;
static uint32_t t_fell;
static int      stopped_after_event;
static uint8_t  prev_bump, prev_cliff;
static uint16_t prev_gpo = 0xFFFF;
static int      prev_section;

static volatile LONG progress;

static const char *led_name(int v) { static const char *n[] = { "OFF", "RED", "GREEN", "YELLOW" }; return n[v & 3]; }

/* -------------------------------------------------------------------- */
/* Logging                                                               */
/* -------------------------------------------------------------------- */


void sim_event(const char *kind, const char *fmt, ...)
{
    va_list ap;
    if (n_events == cap_events) {
        cap_events = cap_events ? cap_events * 2 : 256;
        events = realloc(events, cap_events * sizeof *events);
    }
    Event *e = &events[n_events++];
    e->t = rb.t_ms;
    e->trial = ti;
    snprintf(e->kind, sizeof e->kind, "%s", kind);
    va_start(ap, fmt);
    vsnprintf(e->msg, sizeof e->msg, fmt, ap);
    va_end(ap);
    if (!cfg.quiet && (cfg.html >= 0)) {
        int noisy = !strcmp(kind, "bump") || !strcmp(kind, "cliff");
        if (!noisy || cfg.html == 2)
            printf("  t=%7.2fs  %-6s %s\n", rb.t_ms / 1000.0 - res[ti].t_b0 * (res[ti].t_b0 > 0), kind, e->msg);
    }
}

void sim_warn(const char *key, const char *fmt, ...)
{
    va_list ap;
    for (int i = 0; i < n_warn; i++) if (!strcmp(warn_keys[i], key)) return;
    if (n_warn >= 64) return;
    snprintf(warn_keys[n_warn], sizeof warn_keys[0], "%s", key);
    va_start(ap, fmt);
    vsnprintf(warn_msgs[n_warn], sizeof warn_msgs[0], fmt, ap);
    va_end(ap);
    if (!cfg.quiet) printf("  WARNING: %s\n", warn_msgs[n_warn]);
    n_warn++;
}

/* -------------------------------------------------------------------- */
/* Callbacks from the robot model                                        */
/* -------------------------------------------------------------------- */

void sim_on_command(int16_t speed, int16_t radius, double wl, double wr)
{
    double m = fmax(fabs(wl), fabs(wr));
    (void)speed; (void)radius;
    if (m > res[ti].max_wheel) res[ti].max_wheel = m;
    if (m > SPEED_LIMIT_MMS + 1e-6 && !res[ti].speed_viol) {
        res[ti].speed_viol = 1;
        sim_event("SPEED", "speed limit exceeded: wheel command %.0f mm/s", m);
    }
}

void sim_on_contact(const char *name, Obj_Kind kind)
{
    Trial_Result *r = &res[ti];
    if (kind == OBJ_BOLLARD) {
        r->bollard_hits++;
        if (!strstr(r->hit_names, name) && strlen(r->hit_names) < sizeof r->hit_names - 4) {
            if (r->hit_names[0]) strcat(r->hit_names, ",");
            strcat(r->hit_names, name);
        }
        sim_event("HIT", "hit bollard %s", name);
    } else {
        r->marker_hit = 1;
        sim_event("HIT", "drove into deposit marker %s", name);
    }
}

void sim_on_fall(void)
{
    res[ti].fell = 1;
    t_fell = rb.t_ms;
    sim_event("FELL", "fell off the ledge at (%.0f, %.0f)", rb.x, rb.y);
}

/* -------------------------------------------------------------------- */
/* Results                                                               */
/* -------------------------------------------------------------------- */

typedef struct { char name[64]; int pass; char value[64]; } Check;

static int trial_checks(const Trial_Result *r, Check *c)
{
    int n = 0;
    char side = (r->spec.layout == 'A') ? '4' : '2';
    int  si = side - '0';

#define CHECK(ok, ...) do { c[n].pass = (ok); snprintf(c[n].name, sizeof c[n].name, __VA_ARGS__); n++; } while (0)
#define VALUE(...)     snprintf(c[n - 1].value, sizeof c[n - 1].value, __VA_ARGS__)

    if (r->safety) {
        int ok = r->safety_cmd_ms >= 0 && r->safety_cmd_ms <= 200 && !r->safety_restart;
        CHECK(ok, "Safety: stops on %s", r->safety == 1 ? "B1 press" : "wheel drop (lifted)");
        if (r->safety_cmd_ms < 0) VALUE("never stopped");
        else VALUE("motors 0 after %.0f ms, still after %.0f ms%s", r->safety_cmd_ms, r->safety_stop_ms,
                   r->safety_restart ? ", then RESTARTED" : "");
    } else {
        CHECK(r->t_sec[si] >= 0 && r->t_sec[si] <= 30.0, "Enter section %c within 30 s", side);
        if (r->t_sec[si] >= 0) VALUE("%.1f s", r->t_sec[si]); else VALUE("never (went via %d)", r->side_sec);
        CHECK(r->t_sec[3] >= 0 && r->t_sec[3] <= 60.0, "Enter section 3 within 60 s");
        if (r->t_sec[3] >= 0) VALUE("%.1f s", r->t_sec[3]); else VALUE("never");
        CHECK(r->in_storage, "Entire Kobuki within the storage unit");
        VALUE(r->in_storage ? "%.0f mm clearance" : "%.0f mm outside", fabs(r->margin));
        CHECK(r->bollard_hits == 0, "Avoided hitting any bollard");
        VALUE(r->bollard_hits ? "hit %s" : "no contact", r->hit_names);
        CHECK(fabs(r->gap - 100.0) <= cfg.gap_tol && !r->marker_hit, "Stop 100 mm from deposit D%d", r->spec.deposit);
        VALUE("gap %.0f mm (tol +-%.0f)%s", r->gap, cfg.gap_tol, r->marker_hit ? ", touched it" : "");
        CHECK(fabs(r->align) <= cfg.align_tol, "Stop aligned with deposit D%d", r->spec.deposit);
        VALUE("%+.1f deg (tol +-%.0f)", r->align, cfg.align_tol);
    }
    CHECK(!r->speed_viol, "Never exceed 150 mm/s");
    VALUE("max wheel %.0f mm/s", r->max_wheel);
    CHECK(!r->fell, "Stayed on the platform");
    VALUE(r->fell ? "fell off" : "yes");
    return n;
#undef CHECK
#undef VALUE
}

static void finalize_trial(const char *outcome)
{
    Trial_Result *r = &res[ti];
    const Circle *m = active_marker();
    Check c[16];

    snprintf(r->outcome, sizeof r->outcome, "%s", outcome);
    r->t_end = (rb.t_ms - t_b0_ms) / 1000.0;
    r->end_x = rb.x; r->end_y = rb.y; r->end_th = DEG(rb.th);
    r->in_storage = robot_in_storage(&r->margin);
    if (m) {
        double d = hypot(m->x - rb.x, m->y - rb.y);
        r->gap = d - ROBOT_R - m->r;
        double a = atan2(m->y - rb.y, m->x - rb.x) - rb.th;
        while (a >  SIM_PI) a -= 2 * SIM_PI;
        while (a < -SIM_PI) a += 2 * SIM_PI;
        r->align = DEG(a);
    }
    r->led1 = (rb.gpo >> 8) & 3;
    r->led2 = (rb.gpo >> 10) & 3;

    if (ti == 0) driver_problems = periph_check_config(driver_summary, sizeof driver_summary);

    r->n_checks = trial_checks(r, c);
    r->n_pass = 0;
    for (int i = 0; i < r->n_checks; i++) r->n_pass += c[i].pass;

    if (!cfg.quiet) {
        if (ti == 0)
            printf("\n  Drivers: %s%s\n", driver_summary,
                   driver_problems ? "  (PROBLEMS - see warnings)" : "  (config OK)");
        printf("\n  Trial %d result: %s after %.1f s, LEDs %s / %s, at (%.0f, %.0f) facing %.0f deg\n",
               ti + 1, r->outcome, r->t_end, led_name(r->led1), led_name(r->led2), r->end_x, r->end_y, r->end_th);
        for (int i = 0; i < r->n_checks; i++)
            printf("    [%s] %-40s %s\n", c[i].pass ? "PASS" : "FAIL", c[i].name, c[i].value);
        printf("\n");
    }
    trials_done = ti + 1;
}

/* -------------------------------------------------------------------- */
/* Output: JSON (results + trace) and the HTML replay                    */
/* -------------------------------------------------------------------- */

static void json_str(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { fputc('\\', f); fputc(*s, f); }
        else if ((unsigned char)*s < 0x20) fputc(' ', f);
        else fputc(*s, f);
    }
    fputc('"', f);
}

static void write_results(FILE *f, int with_trace)
{
    const Circle *objs;
    const Rect *w;
    int nw;
    Check c[16];

    fprintf(f, "{\"version\":1,\"seed\":%u,\"driver\":", cfg.seed);
    json_str(f, driver_summary);
    fprintf(f, ",\"driver_problems\":%d", driver_problems);
    fprintf(f, ",\"config\":{\"sensor_offset\":%g,\"beam_half_deg\":%g,\"us_noise\":%g,\"us_latency_ms\":%g,"
               "\"gyro_drift_dpm\":%g,\"accel\":%g,\"gap_tol\":%g,\"align_tol\":%g,\"timeout\":%g}",
            cfg.sensor_offset, cfg.beam_half_deg, cfg.us_noise_mm, cfg.us_latency_ms,
            cfg.gyro_drift_dpm, cfg.accel, cfg.gap_tol, cfg.align_tol, cfg.timeout_s);

    fprintf(f, ",\"warnings\":[");
    for (int i = 0; i < n_warn; i++) { if (i) fputc(',', f); json_str(f, warn_msgs[i]); }
    fprintf(f, "]");

    fprintf(f, ",\"trials\":[");
    for (int i = 0; i < trials_done; i++) {
        Trial_Result *r = &res[i];
        int n = trial_checks(r, c);
        if (i) fputc(',', f);
        fprintf(f, "{\"layout\":\"%c\",\"deposit\":%d,\"heading\":%.1f,\"t_b0\":%.2f,\"outcome\":",
                r->spec.layout, r->spec.deposit, r->spec.heading_deg, r->t_b0);
        json_str(f, r->outcome);
        fprintf(f, ",\"t_end\":%.2f,\"side_section\":%d,\"t_sec2\":%.2f,\"t_sec3\":%.2f,\"t_sec4\":%.2f,"
                   "\"in_storage\":%d,\"margin\":%.1f,\"gap\":%.1f,\"align\":%.2f,\"bollard_hits\":%d,"
                   "\"marker_hit\":%d,\"fell\":%d,\"max_wheel\":%.1f,\"speed_viol\":%d,\"led1\":%d,\"led2\":%d,"
                   "\"end\":[%.1f,%.1f,%.1f],\"safety\":%d,\"safety_cmd_ms\":%.0f,\"safety_stop_ms\":%.0f,"
                   "\"safety_restart\":%d,\"n_checks\":%d,\"n_pass\":%d,\"checks\":[",
                r->t_end, r->side_sec, r->t_sec[2], r->t_sec[3], r->t_sec[4], r->in_storage, r->margin,
                r->gap, r->align, r->bollard_hits, r->marker_hit, r->fell, r->max_wheel, r->speed_viol,
                r->led1, r->led2, r->end_x, r->end_y, r->end_th, r->safety, r->safety_cmd_ms,
                r->safety_stop_ms, r->safety_restart, r->n_checks, r->n_pass);
        for (int k = 0; k < n; k++) {
            if (k) fputc(',', f);
            fprintf(f, "{\"name\":");   json_str(f, c[k].name);
            fprintf(f, ",\"value\":");  json_str(f, c[k].value);
            fprintf(f, ",\"pass\":%s}", c[k].pass ? "true" : "false");
        }
        fprintf(f, "]}");
    }
    fprintf(f, "]");

    if (!with_trace) { fprintf(f, "}\n"); return; }

    w = world_walls(&nw);
    fprintf(f, ",\"arena\":{\"w\":%g,\"h\":%g,\"robot_r\":%g,\"sensor_offset\":%g,\"beam\":%g,\"walls\":[",
            PLAT_W, PLAT_H, ROBOT_R, cfg.sensor_offset, cfg.beam_half_deg);
    for (int i = 0; i < nw; i++)
        fprintf(f, "%s[%g,%g,%g,%g]", i ? "," : "", w[i].x0, w[i].y0, w[i].x1, w[i].y1);
    fprintf(f, "],\"all\":[[\"A1\",195,795],[\"A2\",855,1605],[\"B1\",645,195],[\"B2\",1405,1055],"
               "[\"D1\",615,1285],[\"D2\",985,655]]}");
    (void)objs;

    fprintf(f, ",\"events\":[");
    for (size_t i = 0; i < n_events; i++) {
        fprintf(f, "%s[%u,%d,", i ? "," : "", events[i].t, events[i].trial);
        json_str(f, events[i].kind); fputc(',', f);
        json_str(f, events[i].msg);  fputc(']', f);
    }
    fprintf(f, "]");

    /* t,trial,x,y,th,range,adc,gpo,bump,cliff,drop,btn,cmd_v,cmd_r,vl,vr,gyro,enc,lifted */
    fprintf(f, ",\"frames\":[");
    for (size_t i = 0; i < n_frames; i++) {
        Frame *q = &frames[i];
        fprintf(f, "%s[%u,%u,%.1f,%.1f,%.1f,%u,%u,%u,%u,%u,%u,%u,%d,%d,%.0f,%.0f,%d,%u,%u]",
                i ? "," : "", q->t, q->trial, q->x, q->y, q->th, q->range, q->adc, q->gpo,
                q->bump, q->cliff, q->drop, q->btn, q->cmd_v, q->cmd_r, q->vl, q->vr, q->gyro, q->enc, q->lifted);
    }
    fprintf(f, "]}");
}

static void path_join(char *out, size_t n, const char *a, const char *b) { snprintf(out, n, "%s\\%s", a, b); }

static void write_html(void)
{
    char tpl_path[600], out_path[600], name[200];
    FILE *t, *o;
    long size;
    char *tpl, *mark;
    const char *needle = "/*__SIM_DATA__*/null";

    path_join(tpl_path, sizeof tpl_path, cfg.exe_dir, "viewer\\viewer.html");
    t = fopen(tpl_path, "rb");
    if (!t) { printf("  (no replay written: %s not found)\n", tpl_path); return; }
    fseek(t, 0, SEEK_END); size = ftell(t); fseek(t, 0, SEEK_SET);
    tpl = malloc((size_t)size + 1);
    fread(tpl, 1, (size_t)size, t); tpl[size] = 0;
    fclose(t);
    mark = strstr(tpl, needle);
    if (!mark) { printf("  (viewer template has no data marker)\n"); free(tpl); return; }

    CreateDirectoryA(cfg.out_dir, NULL);
    snprintf(name, sizeof name, "%s.html", cfg.tag);
    path_join(out_path, sizeof out_path, cfg.out_dir, name);
    o = fopen(out_path, "wb");
    if (!o) { printf("  (could not write %s)\n", out_path); free(tpl); return; }
    fwrite(tpl, 1, (size_t)(mark - tpl), o);
    write_results(o, 1);
    fputs(mark + strlen(needle), o);
    fclose(o);
    free(tpl);
    if (!cfg.quiet) printf("  Replay: %s\n", out_path);
}

static void finish(int code)
{
    mmio_unprotect_all();
    if (cfg.json_path[0]) {
        FILE *f = fopen(cfg.json_path, "wb");
        if (f) { write_results(f, 0); fclose(f); }
    }
    if (cfg.html) write_html();
    if (code == 0)
        for (int i = 0; i < trials_done; i++) if (res[i].n_pass != res[i].n_checks) code = 1;
    fflush(stdout);
    exit(code);
}

void sim_fatal(const char *fmt, ...)
{
    char msg[300];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    printf("\n  SIMULATION STOPPED: %s\n", msg);
    sim_event("ERROR", "%s", msg);
    if (trials_done <= ti) finalize_trial(msg);
    finish(2);
}

/* -------------------------------------------------------------------- */
/* Scenario: called every time the firmware wants the next packet        */
/* -------------------------------------------------------------------- */

static void start_trial(int i, int first)
{
    ti = i;
    memset(&res[i], 0, sizeof res[i]);
    res[i].spec = cfg.trials[i];
    res[i].side_sec = 0;
    for (int k = 0; k < 5; k++) res[i].t_sec[k] = -1.0;
    res[i].safety = (cfg.b1_at_s >= 0) ? 1 : (cfg.lift_at_s >= 0) ? 2 : 0;
    res[i].safety_cmd_ms = res[i].safety_stop_ms = -1.0;

    world_setup_trial(&cfg.trials[i], first);
    t_place      = rb.t_ms;
    lifted_until = first ? 0 : rb.t_ms + 1000;       /* carried back to start */
    t_b0_ms      = rb.t_ms + (first ? 500 : 2000);
    res[i].t_b0  = t_b0_ms / 1000.0;
    t_safety = t_safety_end = 0;
    if (cfg.b1_at_s >= 0)   { t_safety = t_b0_ms + (uint32_t)(cfg.b1_at_s * 1000);   t_safety_end = t_safety + 300; }
    if (cfg.lift_at_s >= 0) { t_safety = t_b0_ms + (uint32_t)(cfg.lift_at_s * 1000); t_safety_end = t_safety + (uint32_t)(cfg.lift_for_s * 1000); }
    still_since = 0;
    t_fell = 0;
    stopped_after_event = 0;
    prev_section = section_of(rb.x, rb.y);

    if (!cfg.quiet)
        printf("Trial %d: layout %c, deposit D%d, start heading %.0f deg (world, 0 = +x / right, CCW)\n",
               i + 1, res[i].spec.layout, res[i].spec.deposit, res[i].spec.heading_deg);
    if (!first) sim_event("place", "robot carried back to the start for layout %c / D%d", res[i].spec.layout, res[i].spec.deposit);
}

static void apply_inputs(void)
{
    uint32_t t = rb.t_ms;
    uint8_t btn = 0;

    if (t >= t_b0_ms && t < t_b0_ms + 200) btn |= 0x01;
    if (res[ti].safety == 1 && t >= t_safety && t < t_safety_end) btn |= 0x02;
    rb.button = btn;

    rb.lifted = (t < lifted_until) ||
                (res[ti].safety == 2 && t >= t_safety && t < t_safety_end);

    if (t == t_b0_ms) sim_event("button", "B0 pressed (start)");
    if (res[ti].safety == 1 && t == t_safety) sim_event("button", "B1 pressed (stop)");
    if (res[ti].safety == 2 && t == t_safety) sim_event("lift", "robot lifted (wheel drop)");
    if (res[ti].safety == 2 && t == t_safety_end) sim_event("lift", "robot put back down");
}

static void record_frame(void)
{
    if (n_frames == cap_frames) {
        cap_frames = cap_frames ? cap_frames * 2 : 4096;
        frames = realloc(frames, cap_frames * sizeof *frames);
    }
    Frame *q = &frames[n_frames++];
    double gyro = rb.th - rb.gyro_zero;
    while (gyro >  SIM_PI) gyro -= 2 * SIM_PI;
    while (gyro < -SIM_PI) gyro += 2 * SIM_PI;
    q->t = rb.t_ms; q->trial = (uint8_t)ti;
    q->x = (float)rb.x; q->y = (float)rb.y; q->th = (float)DEG(rb.th);
    q->range = rb.us_range; q->adc = rb.us_adc; q->gpo = rb.gpo;
    q->bump = rb.bumper; q->cliff = rb.cliff; q->drop = rb.wheeldrop; q->btn = rb.button;
    q->lifted = (uint8_t)(rb.lifted || rb.fallen);
    q->cmd_v = rb.cmd_speed; q->cmd_r = rb.cmd_radius;
    q->vl = (float)rb.vl; q->vr = (float)rb.vr;
    q->gyro = (int16_t)lround(DEG(gyro) * 100.0);
    q->enc = (uint16_t)((int64_t)floor(rb.enc_l) & 0xFFFF);
}

static void track_events(void)
{
    static const char *side3[] = { "", "right", "centre", "", "left" };
    Trial_Result *r = &res[ti];
    double since_b0 = (rb.t_ms - (double)t_b0_ms) / 1000.0;

    if (rb.gpo != prev_gpo) {
        if ((rb.gpo & 0x0F00) != (prev_gpo & 0x0F00) || prev_gpo == 0xFFFF)
            sim_event("LED", "LED1 %s, LED2 %s", led_name((rb.gpo >> 8) & 3), led_name((rb.gpo >> 10) & 3));
        prev_gpo = rb.gpo;
    }
    for (int b = 1; b <= 4; b <<= 1) {
        if ((rb.bumper & b) && !(prev_bump & b)) sim_event("bump", "%s bumper pressed", side3[b]);
        if ((rb.cliff & b) && !(prev_cliff & b) && !rb.lifted && !rb.fallen) sim_event("cliff", "%s cliff sensor sees the ledge", side3[b]);
    }
    prev_bump = rb.bumper; prev_cliff = rb.cliff;

    if (rb.t_ms >= t_b0_ms && !rb.lifted) {
        int s = section_of(rb.x, rb.y);
        if (s != prev_section) {
            if (r->t_sec[s] < 0) {
                r->t_sec[s] = since_b0;
                sim_event("section", "entered section %d", s);
            }
            if ((s == 2 || s == 4) && !r->side_sec) r->side_sec = s;
            prev_section = s;
        }
    }

    /* safety: how quickly the motors are told to stop, and do they stay stopped */
    if (r->safety && rb.t_ms >= t_safety) {
        double ms = rb.t_ms - (double)t_safety;
        int cmd_zero = rb.tl == 0.0 && rb.tr == 0.0;
        int still = fabs(rb.vl) < 1.0 && fabs(rb.vr) < 1.0;
        if (r->safety_cmd_ms < 0 && cmd_zero) r->safety_cmd_ms = ms;
        if (r->safety_stop_ms < 0 && cmd_zero && still) { r->safety_stop_ms = ms; stopped_after_event = 1; }
        if (stopped_after_event && !cmd_zero && !r->safety_restart) {
            r->safety_restart = 1;
            sim_event("SAFETY", "motors restarted after the safety stop without B0");
        }
    }
}

void sim_feed_needed(void)
{
    InterlockedIncrement(&progress);

    apply_inputs();
    world_tick();
    track_events();
    record_frame();

    /* end of trial? */
    {
        uint32_t t = rb.t_ms;
        int moving = !(rb.tl == 0.0 && rb.tr == 0.0 && fabs(rb.vl) < 0.5 && fabs(rb.vr) < 0.5);
        const char *why = NULL;

        if (moving || rb.lifted || t < t_b0_ms + 2000 || t < t_safety_end) still_since = 0;
        else if (!still_since) still_since = t;

        if (res[ti].fell && t >= t_fell + 1000)                         why = "fell off the ledge";
        else if (still_since && t - still_since >= 3000)                why = "stopped";
        else if (t >= t_b0_ms + (uint32_t)(cfg.timeout_s * 1000.0))     why = "timed out";

        if (why) {
            if (!strcmp(why, "stopped")) {
                /* report the moment it stopped, not 3 s later */
                rb.t_ms = still_since;
                finalize_trial(why);
                rb.t_ms = t;
            } else {
                finalize_trial(why);
            }
            if (ti + 1 < cfg.n_trials) start_trial(ti + 1, 0);
            else finish(0);
        }
    }
}

/* -------------------------------------------------------------------- */
/* Watchdog: firmware stopped talking to the Kobuki                     */
/* -------------------------------------------------------------------- */

static DWORD WINAPI watchdog(LPVOID arg)
{
    LONG last = -1;
    int idle = 0;
    (void)arg;
    for (;;) {
        Sleep(1000);
        LONG p = progress;
        if (p != last) { last = p; idle = 0; continue; }
        if (++idle >= 5) {
            mmio_unprotect_all();
            sim_fatal(p == 0
                ? "firmware never asked the Kobuki for data (stuck before the main loop? check the warnings)"
                : "firmware stopped reading the Kobuki for 5 s of real time - stuck in a loop?");
        }
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/* Suite: many runs in parallel child processes                          */
/* -------------------------------------------------------------------- */

static double json_num(const char *s, const char *key)
{
    char k[64];
    snprintf(k, sizeof k, "\"%s\":", key);
    const char *p = strstr(s, k);
    return p ? atof(p + strlen(k)) : -999.0;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1);
    fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f);
    return b;
}

typedef struct { char args[400]; char tag[64]; char label[48]; } Job;

static int run_suite(int argc, char **argv, int jobs)
{
    static Job job[64];
    int nj = 0;
    char exe[600], common[1200] = "";
    const int heads[8] = { 0, 45, 90, 135, 180, 225, 270, 315 };

    GetModuleFileNameA(NULL, exe, sizeof exe);
    /* pass through tuning options */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--suite")) continue;
        if (!strcmp(argv[i], "--jobs") || !strcmp(argv[i], "--seed") ||
            !strcmp(argv[i], "--out")) { i++; continue; }      /* set per run */
        strcat(common, " \""); strcat(common, argv[i]); strcat(common, "\"");
    }

    for (int L = 0; L < 2; L++)
        for (int D = 1; D <= 2; D++)
            for (int h = 0; h < 8; h++) {
                Job *j = &job[nj];
                char lay = L ? 'B' : 'A';
                snprintf(j->tag, sizeof j->tag, "suite_%c%d_h%03d", lay, D, heads[h]);
                snprintf(j->label, sizeof j->label, "%c / D%d  heading %3d", lay, D, heads[h]);
                snprintf(j->args, sizeof j->args, "--layout %c --deposit %d --heading %d --seed %u",
                         lay, D, heads[h], cfg.seed + (unsigned)nj);
                nj++;
            }
    for (int L = 0; L < 2; L++) {
        char lay = L ? 'B' : 'A';
        snprintf(job[nj].tag, sizeof job[nj].tag, "suite_%c_safety_b1", lay);
        snprintf(job[nj].label, sizeof job[nj].label, "%c  safety: B1 at 6 s", lay);
        snprintf(job[nj].args, sizeof job[nj].args, "--layout %c --heading 60 --b1-at 6 --seed %u", lay, cfg.seed + (unsigned)nj);
        nj++;
        snprintf(job[nj].tag, sizeof job[nj].tag, "suite_%c_safety_lift", lay);
        snprintf(job[nj].label, sizeof job[nj].label, "%c  safety: lift at 6 s", lay);
        snprintf(job[nj].args, sizeof job[nj].args, "--layout %c --heading 60 --lift-at 6 --seed %u", lay, cfg.seed + (unsigned)nj);
        nj++;
    }

    CreateDirectoryA(cfg.out_dir, NULL);
    printf("Running %d simulations (%d at a time)...\n", nj, jobs);

    HANDLE running[64];
    int    which[64], nr = 0, next = 0, done = 0;
    while (done < nj) {
        while (nr < jobs && next < nj) {
            char cmd[2400];
            STARTUPINFOA si = { sizeof si };
            PROCESS_INFORMATION pi;
            snprintf(cmd, sizeof cmd, "\"%s\" %s --quiet --tag %s --json \"%s\\%s.json\" --out \"%s\"%s",
                     exe, job[next].args, job[next].tag, cfg.out_dir, job[next].tag, cfg.out_dir, common);
            if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                printf("  could not start %s\n", job[next].tag);
                next++; done++;
                continue;
            }
            CloseHandle(pi.hThread);
            running[nr] = pi.hProcess; which[nr] = next; nr++; next++;
        }
        DWORD w = WaitForMultipleObjects((DWORD)nr, running, FALSE, INFINITE) - WAIT_OBJECT_0;
        CloseHandle(running[w]);
        running[w] = running[nr - 1]; which[w] = which[nr - 1]; nr--;
        done++;
        printf("\r  %d / %d done", done, nj);
        fflush(stdout);
    }
    printf("\n\n");

    /* table */
    char idx_path[600];
    snprintf(idx_path, sizeof idx_path, "%s\\index.html", cfg.out_dir);
    FILE *ix = fopen(idx_path, "wb");
    if (ix) fprintf(ix, "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
                        "<title>Kobuki Sim Suite</title><style>:root{color-scheme:light dark;--bg:#f7f7f5;--fg:#1d1d1b;--mut:#6b6b66;--line:#ddd;--ok:#1a7f37;--bad:#c62828}"
                        "@media(prefers-color-scheme:dark){:root{--bg:#161616;--fg:#e8e8e6;--mut:#9a9a95;--line:#333;--ok:#4cc26b;--bad:#ff6b6b}}"
                        "body{background:var(--bg);color:var(--fg);font:14px/1.45 system-ui,sans-serif;margin:0;padding:24px 16px}"
                        "main{max-width:1100px;margin:auto}h1{font-size:20px;margin:0 0 4px}p{color:var(--mut);margin:0 0 16px}"
                        ".wrap{overflow-x:auto}table{border-collapse:collapse;width:100%%;font-variant-numeric:tabular-nums}"
                        "th,td{padding:6px 10px;border-bottom:1px solid var(--line);text-align:left;white-space:nowrap}"
                        "th{color:var(--mut);font-weight:600;font-size:12px}.ok{color:var(--ok)}.bad{color:var(--bad);font-weight:600}a{color:inherit}</style>"
                        "<main><h1>Kobuki simulator &ndash; test suite</h1><p>Seed %u. Click a run to open its replay.</p><div class=wrap><table>"
                        "<tr><th>Run</th><th>Outcome</th><th>Path</th><th>Side sec</th><th>Sec 3</th><th>In storage</th>"
                        "<th>Bollards</th><th>Gap</th><th>Align</th><th>Max wheel</th><th>Checks</th></tr>", cfg.seed);

    printf("  %-24s %-18s %4s %7s %7s %8s %7s %7s %8s %6s %s\n",
           "run", "outcome", "path", "side", "sec3", "storage", "bollard", "gap", "align", "maxv", "checks");
    int total_pass = 0, total = 0, runs_ok = 0;
    for (int i = 0; i < nj; i++) {
        char p[600];
        snprintf(p, sizeof p, "%s\\%s.json", cfg.out_dir, job[i].tag);
        char *s = read_file(p);
        if (!s) { printf("  %-24s (no result - crashed?)\n", job[i].label); continue; }
        char outcome[64] = "?";
        const char *o = strstr(s, "\"outcome\":\"");
        if (o) { o += 11; int k = 0; while (*o && *o != '"' && k < 63) outcome[k++] = *o++; outcome[k] = 0; }
        int    side = (int)json_num(s, "side_section");
        double t2 = json_num(s, "t_sec2"), t3 = json_num(s, "t_sec3"), t4 = json_num(s, "t_sec4");
        double tside = side == 2 ? t2 : (side == 4 ? t4 : -1);
        int    ins = (int)json_num(s, "in_storage"), hits = (int)json_num(s, "bollard_hits");
        double gap = json_num(s, "gap"), al = json_num(s, "align"), mv = json_num(s, "max_wheel");
        int    nc = (int)json_num(s, "n_checks"), np = (int)json_num(s, "n_pass");
        int    safety = (int)json_num(s, "safety");
        total += nc; total_pass += np; runs_ok += (nc == np);

        char sidebuf[16], s3buf[16], gapbuf[16], albuf[16];
        snprintf(sidebuf, sizeof sidebuf, tside >= 0 ? "%.1fs" : "-", tside);
        snprintf(s3buf, sizeof s3buf, t3 >= 0 ? "%.1fs" : "-", t3);
        snprintf(gapbuf, sizeof gapbuf, "%.0fmm", gap);
        snprintf(albuf, sizeof albuf, "%+.1f", al);
        if (safety) { strcpy(gapbuf, "-"); strcpy(albuf, "-"); }
        printf("  %-24s %-18.18s %4d %7s %7s %8s %7s %7s %8s %6.0f %d/%d%s\n",
               job[i].label, outcome, side, sidebuf, s3buf, safety ? "-" : (ins ? "yes" : "no"),
               hits ? "HIT" : "ok", gapbuf, albuf, mv, np, nc, np == nc ? "" : "  <--");
        if (ix) fprintf(ix, "<tr><td><a href='%s.html'>%s</a></td><td>%s</td><td>%d</td><td>%s</td><td>%s</td>"
                            "<td class=%s>%s</td><td class=%s>%s</td><td>%s</td><td>%s</td><td>%.0f</td><td class=%s>%d/%d</td></tr>",
                        job[i].tag, job[i].label, outcome, side, sidebuf, s3buf,
                        ins || safety ? "ok" : "bad", safety ? "-" : (ins ? "yes" : "no"),
                        hits ? "bad" : "ok", hits ? "HIT" : "ok", gapbuf, albuf, mv,
                        np == nc ? "ok" : "bad", np, nc);
        free(s);
    }
    printf("\n  %d / %d runs passed every check  (%d / %d checks)\n", runs_ok, nj, total_pass, total);
    printf("  Summary with replay links: %s\n", idx_path);
    if (ix) { fprintf(ix, "</table></div><p style='margin-top:16px'>%d / %d runs passed every check (%d / %d checks).</p></main>", runs_ok, nj, total_pass, total); fclose(ix); }
    return runs_ok == nj ? 0 : 1;
}

/* -------------------------------------------------------------------- */
/* Command line                                                          */
/* -------------------------------------------------------------------- */

static void usage(void)
{
    printf(
"Kobuki simulator - runs the Src/ firmware against a simulated Kobuki + MB1043\n"
"\n"
"  kobuki_sim [options]\n"
"\n"
"Scenario\n"
"  --layout A|B           bollard layout (default A)\n"
"  --deposit 1|2          deposit marker (default: D2 for A, D1 for B, as in Table 1)\n"
"  --heading DEG|rand     start heading, world frame: 0 = facing right (+x), 90 = up (default rand)\n"
"  --trials A2,B1@90,...  back-to-back runs without restarting the firmware (final test)\n"
"  --b1-at SEC            press B1 SEC seconds after B0 (safety check)\n"
"  --lift-at SEC          lift the robot SEC seconds after B0 (wheel drop safety check)\n"
"  --lift-for SEC         how long it is held up (default 2)\n"
"  --timeout SEC          per-trial time limit (default 120)\n"
"  --seed N               random seed (default: time); printed so runs can be repeated\n"
"  --suite                every layout x deposit x 8 headings + safety checks, in parallel\n"
"  --jobs N               parallel runs for --suite (default: CPU count)\n"
"\n"
"Sensor / robot model\n"
"  --sensor-offset MM     ultrasonic distance in front of the robot centre (default 0 = at the centre)\n"
"  --beam DEG             ultrasonic half-angle that still sees a 90 mm bollard (default 10)\n"
"  --us-noise MM          ultrasonic noise, 1 sigma (default 3)\n"
"  --us-latency MS        delay before a reading appears on the analogue pin (default 100)\n"
"  --gyro-drift DEG/MIN   (default 0)\n"
"  --accel MM/S2          wheel acceleration limit (default 1000)\n"
"  --start-jitter MM      random error in the start position (default 0)\n"
"\n"
"Scoring / output\n"
"  --gap-tol MM           allowed error on the 100 mm stop distance (default 25)\n"
"  --align-tol DEG        allowed alignment error (default 5)\n"
"  --strict               stop if the UART/ADC drivers are misconfigured\n"
"  --out DIR              output folder (default sim_out)\n"
"  --no-html              skip the HTML replay\n"
"  -v                     also print every bumper / cliff event\n"
"  --quiet\n");
}

static int parse_trials(const char *s)
{
    cfg.n_trials = 0;
    while (*s && cfg.n_trials < MAX_TRIALS) {
        Trial_Spec *t = &cfg.trials[cfg.n_trials];
        t->layout = (char)((*s == 'b' || *s == 'B') ? 'B' : 'A');
        s++;
        t->deposit = (*s == '1' || *s == '2') ? (*s++ - '0') : (t->layout == 'A' ? 2 : 1);
        t->heading_deg = sim_rand() * 360.0;
        if (*s == '@') { t->heading_deg = strtod(s + 1, (char **)&s); }
        cfg.n_trials++;
        while (*s == ',' || *s == ' ') s++;
    }
    return cfg.n_trials > 0;
}

int main(int argc, char **argv)
{
    char layout = 'A';
    int  deposit = 0, suite = 0, jobs = 0, seed_set = 0;
    double heading = -1.0;
    const char *trials_arg = NULL;

    setvbuf(stdout, NULL, _IONBF, 0);

    cfg.seed           = (unsigned)time(NULL) % 100000u;
    cfg.timeout_s      = 120.0;
    cfg.b1_at_s        = -1.0;
    cfg.lift_at_s      = -1.0;
    cfg.lift_for_s     = 2.0;
    cfg.sensor_offset  = 0.0;
    cfg.beam_half_deg  = 10.0;
    cfg.us_noise_mm    = 3.0;
    cfg.us_latency_ms  = 100.0;
    cfg.accel          = 1000.0;
    cfg.gap_tol   = 25.0;
    cfg.align_tol = 5.0;
    cfg.html      = 1;
    strcpy(cfg.out_dir, "sim_out");

    {
        char exe[600], *slash;
        GetModuleFileNameA(NULL, exe, sizeof exe);
        slash = strrchr(exe, '\\');
        if (slash) *slash = 0;
        snprintf(cfg.exe_dir, sizeof cfg.exe_dir, "%s", exe);
    }

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = (i + 1 < argc) ? argv[i + 1] : "";
#define ARG(name) (!strcmp(a, name) && (i + 1 < argc) && (i++, 1))
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(); return 0; }
        else if (ARG("--layout"))        layout = (char)((v[0] == 'b' || v[0] == 'B') ? 'B' : 'A');
        else if (ARG("--deposit"))       deposit = atoi(v);
        else if (ARG("--heading"))       heading = strcmp(v, "rand") ? fmod(atof(v) + 3600.0, 360.0) : -1.0;
        else if (ARG("--trials"))        trials_arg = v;
        else if (ARG("--b1-at"))         cfg.b1_at_s = atof(v);
        else if (ARG("--lift-at"))       cfg.lift_at_s = atof(v);
        else if (ARG("--lift-for"))      cfg.lift_for_s = atof(v);
        else if (ARG("--timeout"))       cfg.timeout_s = atof(v);
        else if (ARG("--seed"))          { cfg.seed = (unsigned)strtoul(v, NULL, 10); seed_set = 1; }
        else if (ARG("--sensor-offset")) cfg.sensor_offset = atof(v);
        else if (ARG("--beam"))          cfg.beam_half_deg = atof(v);
        else if (ARG("--us-noise"))      cfg.us_noise_mm = atof(v);
        else if (ARG("--us-latency"))    cfg.us_latency_ms = atof(v);
        else if (ARG("--gyro-drift"))    cfg.gyro_drift_dpm = atof(v);
        else if (ARG("--accel"))         cfg.accel = atof(v);
        else if (ARG("--start-jitter"))  cfg.start_jitter = atof(v);
        else if (ARG("--gap-tol"))       cfg.gap_tol = atof(v);
        else if (ARG("--align-tol"))     cfg.align_tol = atof(v);
        else if (ARG("--out"))           snprintf(cfg.out_dir, sizeof cfg.out_dir, "%s", v);
        else if (ARG("--json"))          snprintf(cfg.json_path, sizeof cfg.json_path, "%s", v);
        else if (ARG("--tag"))           snprintf(cfg.tag, sizeof cfg.tag, "%s", v);
        else if (ARG("--jobs"))          jobs = atoi(v);
        else if (!strcmp(a, "--suite"))   suite = 1;
        else if (!strcmp(a, "--strict"))  cfg.strict = 1;
        else if (!strcmp(a, "--no-html")) cfg.html = 0;
        else if (!strcmp(a, "--quiet"))   cfg.quiet = 1;
        else if (!strcmp(a, "-v"))        cfg.html = cfg.html ? 2 : 0;
        else { printf("unknown option %s (try --help)\n", a); return 2; }
#undef ARG
    }
    (void)seed_set;

    sim_seed(cfg.seed);

    if (suite) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        if (jobs <= 0) jobs = (int)si.dwNumberOfProcessors;
        if (jobs > 32) jobs = 32;
        return run_suite(argc, argv, jobs);
    }


    if (trials_arg) {
        if (!parse_trials(trials_arg)) { printf("bad --trials\n"); return 2; }
    } else {
        cfg.n_trials = 1;
        cfg.trials[0].layout = layout;
        cfg.trials[0].deposit = deposit ? deposit : (layout == 'A' ? 2 : 1);
        cfg.trials[0].heading_deg = heading >= 0 ? heading : floor(sim_rand() * 360.0);
    }
    if (!cfg.tag[0]) {
        char *p = cfg.tag;
        p += sprintf(p, "run");
        for (int i = 0; i < cfg.n_trials; i++)
            p += sprintf(p, "_%c%d_h%03.0f", cfg.trials[i].layout, cfg.trials[i].deposit, cfg.trials[i].heading_deg);
        if (cfg.b1_at_s >= 0) p += sprintf(p, "_b1");
        if (cfg.lift_at_s >= 0) p += sprintf(p, "_lift");
    }

#ifdef SIM_SHIM
    if (!mmio_init(0)) return 2;
#else
    if (!mmio_init(1)) return 2;
#endif

    if (!cfg.quiet)
        printf("Kobuki simulator  seed %u  (ultrasonic %.0f mm ahead of centre, +-%.0f deg beam)\n",
               cfg.seed, cfg.sensor_offset, cfg.beam_half_deg);

    start_trial(0, 1);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);

    firmware_main();
    sim_fatal("firmware main() returned");
    return 2;
}
