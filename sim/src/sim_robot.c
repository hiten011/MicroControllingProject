/*
 * sim_robot.c - the Kobuki, the MB1043 ultrasonic sensor and the arena.
 *
 * Kobuki protocol (Kobuki bytestream document):
 *   MCU -> Kobuki   AA 55 len [id len data]... checksum
 *                   0x01 base control: speed (mm/s), radius (mm)
 *                   0x0C general purpose output: LEDs, power rails
 *   Kobuki -> MCU   every 20 ms: basic sensor data (0x01), docking IR (0x03),
 *                   inertial (0x04), cliff (0x05), current (0x06), GPI (0x10)
 *
 * The robot is a 354 mm disc on two wheels (230 mm apart). Wheels have
 * an acceleration limit; when the body is blocked by a wall the wheels
 * slip, so the encoders keep counting but the gyro does not lie.
 */
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "sim.h"

Robot rb;

/* ---- Arena (Figure 2, mm) ------------------------------------------- */

static const Rect walls[] = {
    /* storage unit, 16 mm walls; entrance is the diagonal (850,1400)-(1200,1050) */
    {  400.0,  400.0,  416.0, 1400.0 },     /* left                       */
    {  400.0, 1384.0,  850.0, 1400.0 },     /* top                        */
    {  400.0,  400.0, 1200.0,  416.0 },     /* bottom                     */
    { 1184.0,  400.0, 1200.0, 1050.0 },     /* right (short)              */
    /* room walls beyond the ledge (only the ultrasonic can see them)     */
    {  -50.0, 2200.0, 2050.0, 2300.0 },     /* 400 mm above the platform  */
    { 1950.0,   45.0, 2050.0, 2300.0 },     /* 350 mm right of it         */
};
#define N_WALLS ((int)(sizeof walls / sizeof walls[0]))
#define N_STORAGE_WALLS 4           /* below the sensor: bumpers only, no echo */

static const Circle catalogue[] = {
    { "A1",  195.0,  795.0, BOLLARD_R, OBJ_BOLLARD },
    { "A2",  855.0, 1605.0, BOLLARD_R, OBJ_BOLLARD },
    { "B1",  645.0,  195.0, BOLLARD_R, OBJ_BOLLARD },
    { "B2", 1405.0, 1055.0, BOLLARD_R, OBJ_BOLLARD },
    { "D1",  615.0, 1285.0, BOLLARD_R, OBJ_MARKER  },
    { "D2",  985.0,  655.0, BOLLARD_R, OBJ_MARKER  },
};

static Circle objs[4];
static int    n_objs;
static int    touching[4];

/* ---- Random numbers (deterministic per seed) ------------------------ */

static uint64_t rng_state = 88172645463325252ull;

void sim_seed(unsigned s) { rng_state = 0x9E3779B97F4A7C15ull ^ ((uint64_t)s * 0x2545F4914F6CDD1Dull); if (!rng_state) rng_state = 1; }

double sim_rand(void)                       /* [0, 1) */
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (double)(rng_state >> 11) / 9007199254740992.0;
}

static double gauss(void)
{
    double u = sim_rand() + 1e-12, v = sim_rand();
    return sqrt(-2.0 * log(u)) * cos(2.0 * SIM_PI * v);
}

static double wrap_pi(double a)
{
    while (a >  SIM_PI) a -= 2.0 * SIM_PI;
    while (a < -SIM_PI) a += 2.0 * SIM_PI;
    return a;
}

static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ---- Ultrasonic timing ---------------------------------------------- */

#define US_PERIOD_MS 100u           /* MB1043 ranges at 10 Hz            */

static uint32_t us_next_meas, us_pub_t;
static uint16_t us_pending;
static int      us_has_pending;

/* -------------------------------------------------------------------- */
/* Arena / setup                                                         */
/* -------------------------------------------------------------------- */

static void add_obj(const char *name)
{
    for (size_t i = 0; i < sizeof catalogue / sizeof catalogue[0]; i++)
        if (strcmp(catalogue[i].name, name) == 0) objs[n_objs++] = catalogue[i];
}

static uint16_t us_measure(void);

void world_setup_trial(const Trial_Spec *t, int first)
{
    n_objs = 0;
    if (t->layout == 'A') { add_obj("A1"); add_obj("A2"); }
    else                  { add_obj("B1"); add_obj("B2"); }
    add_obj(t->deposit == 1 ? "D1" : "D2");
    memset(touching, 0, sizeof touching);

    rb.x  = START_X + (sim_rand() * 2.0 - 1.0) * cfg.start_jitter;
    rb.y  = START_Y + (sim_rand() * 2.0 - 1.0) * cfg.start_jitter;
    rb.th = wrap_pi(RAD(t->heading_deg));
    rb.fallen = 0;

    if (first) {
        /* Gyro reads 0 where the Kobuki was switched on, encoders start
         * anywhere - both arbitrary, which also exercises wrap-around. */
        rb.gyro_zero = (sim_rand() * 2.0 - 1.0) * SIM_PI;
        rb.enc_l = floor(sim_rand() * 65536.0);
        rb.enc_r = floor(sim_rand() * 65536.0);
        rb.vl = rb.vr = rb.tl = rb.tr = 0.0;
        rb.gpo = 0;
        us_next_meas = (uint32_t)(sim_rand() * US_PERIOD_MS);
        us_has_pending = 0;
    }
    rb.us_range = us_measure();
}

int world_objects(const Circle **list) { *list = objs; return n_objs; }
const Rect *world_walls(int *n) { *n = N_WALLS; return walls; }

const Circle *active_marker(void)
{
    for (int i = 0; i < n_objs; i++) if (objs[i].kind == OBJ_MARKER) return &objs[i];
    return NULL;
}

int section_of(double x, double y)
{
    if (x < PLAT_W / 2.0) return (y < PLAT_H / 2.0) ? 1 : 2;
    return (y < PLAT_H / 2.0) ? 4 : 3;
}

/* Smallest clearance between the robot's edge and the storage unit's
 * inside faces / entrance line. >= 0 means the whole Kobuki is inside. */
int robot_in_storage(double *margin)
{
    double m = rb.x - ROBOT_R - 416.0;
    m = fmin(m, 1184.0 - rb.x - ROBOT_R);
    m = fmin(m, rb.y - ROBOT_R - 416.0);
    m = fmin(m, 1384.0 - rb.y - ROBOT_R);
    m = fmin(m, (2250.0 - (rb.x + rb.y)) / sqrt(2.0) - ROBOT_R);
    if (margin) *margin = m;
    return m >= 0.0;
}

double wheel_speed_cmd_max(void) { return fmax(fabs(rb.tl), fabs(rb.tr)); }

/* -------------------------------------------------------------------- */
/* Physics                                                               */
/* -------------------------------------------------------------------- */

static double approach(double v, double target, double dv)
{
    if (v < target) return fmin(v + dv, target);
    return fmax(v - dv, target);
}

static void push_out_rect(const Rect *r)
{
    double cx = clampd(rb.x, r->x0, r->x1), cy = clampd(rb.y, r->y0, r->y1);
    double dx = rb.x - cx, dy = rb.y - cy, d = hypot(dx, dy);
    if (d >= ROBOT_R) return;
    if (d > 1e-9) {
        rb.x = cx + dx / d * ROBOT_R;
        rb.y = cy + dy / d * ROBOT_R;
    } else {                                   /* centre inside the wall */
        double l = rb.x - r->x0, rt = r->x1 - rb.x, b = rb.y - r->y0, t = r->y1 - rb.y;
        double m = fmin(fmin(l, rt), fmin(b, t));
        if (m == l)       rb.x = r->x0 - ROBOT_R;
        else if (m == rt) rb.x = r->x1 + ROBOT_R;
        else if (m == b)  rb.y = r->y0 - ROBOT_R;
        else              rb.y = r->y1 + ROBOT_R;
    }
}

static void push_out_circle(const Circle *c)
{
    double dx = rb.x - c->x, dy = rb.y - c->y, d = hypot(dx, dy), min = ROBOT_R + c->r;
    if (d >= min) return;
    if (d < 1e-9) { dx = 1.0; dy = 0.0; d = 1.0; }
    rb.x = c->x + dx / d * min;
    rb.y = c->y + dy / d * min;
}

static double off_platform(double px, double py)
{
    return fmax(fmax(-px, px - PLAT_W), fmax(-py, py - PLAT_H));
}

static void check_fall(void)
{
    /* wheels at +-115 mm, castors front and back */
    static const double a[4] = { 0.0, SIM_PI / 2.0, SIM_PI, -SIM_PI / 2.0 };
    for (int i = 0; i < 4; i++) {
        double px = rb.x + 115.0 * cos(rb.th + a[i]);
        double py = rb.y + 115.0 * sin(rb.th + a[i]);
        if (off_platform(px, py) > 10.0) {
            rb.fallen = 1;
            sim_on_fall();
            return;
        }
    }
}

static void substep(double dt)
{
    double dv = cfg.accel * dt;
    rb.vl = approach(rb.vl, rb.tl, dv);
    rb.vr = approach(rb.vr, rb.tr, dv);

    double dl = rb.vl * dt, dr = rb.vr * dt;
    rb.enc_l += dl / SIM_MM_PER_TICK;
    rb.enc_r += dr / SIM_MM_PER_TICK;
    if (rb.lifted || rb.fallen) return;          /* wheels spin in the air */

    double ds = 0.5 * (dl + dr), dth = (dr - dl) / WHEEL_BASE;
    double tm = rb.th + 0.5 * dth;
    rb.x  += ds * cos(tm);
    rb.y  += ds * sin(tm);
    rb.th  = wrap_pi(rb.th + dth);

    for (int it = 0; it < 2; it++) {
        for (int i = 0; i < N_WALLS; i++) push_out_rect(&walls[i]);
        for (int i = 0; i < n_objs; i++) push_out_circle(&objs[i]);
    }
    check_fall();
}

/* -------------------------------------------------------------------- */
/* Sensors                                                               */
/* -------------------------------------------------------------------- */

static uint8_t bumper_bit(double px, double py)
{
    double a = wrap_pi(atan2(py - rb.y, px - rb.x) - rb.th);
    if (fabs(a) > RAD(90.0)) return 0;          /* no bumper at the back  */
    if (a >  RAD(30.0)) return 0x04;            /* left                   */
    if (a < -RAD(30.0)) return 0x01;            /* right                  */
    return 0x02;                                /* centre                 */
}

static void update_contacts(void)
{
    uint8_t b = 0;
    if (!rb.lifted && !rb.fallen) {
        for (int i = 0; i < N_WALLS; i++) {
            const Rect *r = &walls[i];
            double cx = clampd(rb.x, r->x0, r->x1), cy = clampd(rb.y, r->y0, r->y1);
            if (hypot(rb.x - cx, rb.y - cy) <= ROBOT_R + 0.5) b |= bumper_bit(cx, cy);
        }
        for (int i = 0; i < n_objs; i++) {
            const Circle *c = &objs[i];
            double d = hypot(rb.x - c->x, rb.y - c->y);
            int now = d <= ROBOT_R + c->r + 0.5;
            if (now) {
                b |= bumper_bit(c->x, c->y);
                if (!touching[i]) sim_on_contact(c->name, c->kind);
            }
            touching[i] = now;
        }
    }
    rb.bumper = b;
}

static void update_cliff_and_drop(void)
{
    /* right, centre, left - just behind the bumper */
    static const double ang[3] = { -50.0, 0.0, 50.0 };
    static const uint8_t bit[3] = { 0x01, 0x02, 0x04 };
    uint8_t c = 0;

    if (rb.lifted || rb.fallen) {
        rb.cliff = 0x07;
        rb.wheeldrop = 0x03;
        return;
    }
    for (int i = 0; i < 3; i++) {
        double px = rb.x + 165.0 * cos(rb.th + RAD(ang[i]));
        double py = rb.y + 165.0 * sin(rb.th + RAD(ang[i]));
        if (off_platform(px, py) > 0.0) c |= bit[i];
    }
    rb.cliff = c;
    rb.wheeldrop = 0;
}

/* MB1043: nearest echo inside the beam. Round targets are seen if any
 * part of them is inside the cone; flat walls only echo back where the
 * wall is square to the sensor (or at a corner). The storage unit's
 * walls are lower than the sensor, so they never echo. */
static uint16_t us_measure(void)
{
    double sx = rb.x + cfg.sensor_offset * cos(rb.th);
    double sy = rb.y + cfg.sensor_offset * sin(rb.th);
    double half = RAD(cfg.beam_half_deg);
    double best = 1e9;

    for (int i = 0; i < n_objs; i++) {
        double dx = objs[i].x - sx, dy = objs[i].y - sy, d = hypot(dx, dy);
        if (d <= objs[i].r) { best = 0.0; continue; }
        double a = fabs(wrap_pi(atan2(dy, dx) - rb.th));
        if (a - asin(objs[i].r / d) <= half) best = fmin(best, d - objs[i].r);
    }

    for (int i = N_STORAGE_WALLS; i < N_WALLS; i++) {
        const Rect *r = &walls[i];
        double cxs[4] = { r->x0, r->x1, r->x1, r->x0 };
        double cys[4] = { r->y0, r->y0, r->y1, r->y1 };
        for (int k = 0; k < 4; k++) {
            /* corner echo */
            double dx = cxs[k] - sx, dy = cys[k] - sy;
            if (fabs(wrap_pi(atan2(dy, dx) - rb.th)) <= half) best = fmin(best, hypot(dx, dy));
        }
        /* face echoes: the perpendicular foot must lie on the face */
        if (sy >= r->y0 && sy <= r->y1) {
            if (sx < r->x0 && fabs(wrap_pi(0.0 - rb.th)) <= half)      best = fmin(best, r->x0 - sx);
            if (sx > r->x1 && fabs(wrap_pi(SIM_PI - rb.th)) <= half)   best = fmin(best, sx - r->x1);
        }
        if (sx >= r->x0 && sx <= r->x1) {
            if (sy < r->y0 && fabs(wrap_pi(SIM_PI / 2.0 - rb.th)) <= half)  best = fmin(best, r->y0 - sy);
            if (sy > r->y1 && fabs(wrap_pi(-SIM_PI / 2.0 - rb.th)) <= half) best = fmin(best, sy - r->y1);
        }
    }

    if (best > 5000.0) best = 5000.0;           /* no target: max range   */
    else best += gauss() * cfg.us_noise_mm;
    best = clampd(best, 300.0, 5000.0);         /* MB1043 floor is 300 mm */
    return (uint16_t)lround(best);
}

uint16_t sensor_adc(int channel)
{
    if (channel != 1) {
        sim_warn("a.ch", "ADC1 channel %d read - nothing is wired to it in the simulator (ultrasonic is IN1/PA0)", channel);
        return (uint16_t)(sim_rand() * 12.0);
    }
    /* Vcc/5120 per mm, 3.3 V supply and reference, 12-bit */
    double code = rb.us_range * 4096.0 / 5120.0 + (sim_rand() * 3.0 - 1.5);
    rb.us_adc = (uint16_t)clampd(lround(code), 0.0, 4095.0);
    return rb.us_adc;
}

/* -------------------------------------------------------------------- */
/* One 20 ms step                                                        */
/* -------------------------------------------------------------------- */

void world_tick(void)
{
    for (int i = 0; i < 5; i++) substep(LOOP_MS / 5.0 / 1000.0);
    rb.t_ms += LOOP_MS;

    update_contacts();
    update_cliff_and_drop();

    if (us_has_pending && rb.t_ms >= us_pub_t) {
        rb.us_range = us_pending;
        us_has_pending = 0;
    }
    if (rb.t_ms >= us_next_meas) {
        us_pending = us_measure();
        us_pub_t = rb.t_ms + (uint32_t)cfg.us_latency_ms;
        us_has_pending = 1;
        us_next_meas += US_PERIOD_MS;
        if (cfg.us_latency_ms <= 0.0) { rb.us_range = us_pending; us_has_pending = 0; }
    }
}

/* -------------------------------------------------------------------- */
/* Kobuki protocol                                                       */
/* -------------------------------------------------------------------- */

static void set_command(int16_t speed, int16_t radius)
{
    double s = speed;
    rb.cmd_speed  = speed;
    rb.cmd_radius = radius;
    if (radius == 0) {                          /* straight               */
        rb.tl = rb.tr = s;
    } else if (radius == 1) {                   /* spin, +ve = CCW        */
        rb.tl = -s; rb.tr = s;
    } else if (radius == -1) {
        rb.tl = s;  rb.tr = -s;
    } else {
        rb.tl = s * (radius - WHEEL_BASE / 2.0) / radius;
        rb.tr = s * (radius + WHEEL_BASE / 2.0) / radius;
    }
    sim_on_command(speed, radius, rb.tl, rb.tr);
}

static void handle_payload(const uint8_t *p, int len)
{
    int i = 0;
    while (i + 2 <= len) {
        uint8_t id = p[i], n = p[i + 1];
        const uint8_t *d = p + i + 2;
        if (i + 2 + n > len) {
            sim_warn("k.trunc", "Kobuki packet: sub-payload 0x%02X claims %d bytes but the packet ends first", id, n);
            return;
        }
        switch (id) {
            case 0x01:
                if (n != 4) sim_warn("k.base", "base control sub-payload should be 4 bytes, got %d", n);
                else set_command((int16_t)(d[0] | (d[1] << 8)), (int16_t)(d[2] | (d[3] << 8)));
                break;
            case 0x0C:
                if (n != 2) sim_warn("k.gpo", "general purpose output sub-payload should be 2 bytes, got %d", n);
                else rb.gpo = (uint16_t)(d[0] | (d[1] << 8));
                break;
            case 0x03: case 0x04: case 0x09: case 0x0D: case 0x0E:
                break;                                  /* sound, gains... */
            default:
                sim_warn("k.id", "unknown Kobuki command id 0x%02X", id);
        }
        i += 2 + n;
    }
}

void kobuki_rx_byte(uint8_t b)
{
    static int     st;
    static uint8_t len, n, cs, buf[256];

    switch (st) {
        case 0: if (b == 0xAA) st = 1; else sim_warn("k.junk", "byte 0x%02X sent to the Kobuki outside a packet", b); break;
        case 1: st = (b == 0x55) ? 2 : (b == 0xAA ? 1 : 0); break;
        case 2: len = b; n = 0; cs = b; st = len ? 3 : 4; break;
        case 3: buf[n++] = b; cs ^= b; if (n >= len) st = 4; break;
        case 4:
            if (b == cs) handle_payload(buf, len);
            else sim_warn("k.cs", "Kobuki packet with a bad checksum (got 0x%02X, expected 0x%02X) - ignored", b, cs);
            st = 0;
            break;
    }
}

static int put16(uint8_t *p, int n, int v) { p[n] = (uint8_t)v; p[n + 1] = (uint8_t)(v >> 8); return n + 2; }

int kobuki_feedback(uint8_t *p)
{
    int n = 0, len_at, i;
    uint8_t cs = 0;
    double minutes = rb.t_ms / 60000.0;
    double gyro = wrap_pi(rb.th - rb.gyro_zero + RAD(cfg.gyro_drift_dpm) * minutes);
    int angle = (int)lround(DEG(gyro) * 100.0);
    if (angle >= 18000) angle -= 36000;
    int rate = (int)lround(DEG((rb.lifted || rb.fallen) ? 0.0 : (rb.vr - rb.vl) / WHEEL_BASE) * 100.0);

    p[n++] = 0xAA; p[n++] = 0x55; len_at = n++;

    /* 0x01 basic sensor data */
    p[n++] = 0x01; p[n++] = 0x0F;
    n = put16(p, n, (int)(rb.t_ms & 0xFFFFu));
    p[n++] = rb.bumper;
    p[n++] = rb.wheeldrop;
    p[n++] = rb.cliff;
    n = put16(p, n, (int)((int64_t)floor(rb.enc_l) & 0xFFFF));
    n = put16(p, n, (int)((int64_t)floor(rb.enc_r) & 0xFFFF));
    p[n++] = (uint8_t)(int8_t)clampd(rb.vl / 7.0, -100, 100);
    p[n++] = (uint8_t)(int8_t)clampd(rb.vr / 7.0, -100, 100);
    p[n++] = rb.button;
    p[n++] = 0;                                 /* charger: discharging   */
    p[n++] = 160;                               /* battery 16.0 V         */
    p[n++] = 0;                                 /* overcurrent            */

    /* 0x03 docking IR */
    p[n++] = 0x03; p[n++] = 0x03; p[n++] = 0; p[n++] = 0; p[n++] = 0;

    /* 0x04 inertial sensor: angle and rate in 0.01 deg (/s) */
    p[n++] = 0x04; p[n++] = 0x07;
    n = put16(p, n, angle);
    n = put16(p, n, rate);
    p[n++] = 0; p[n++] = 0; p[n++] = 0;

    /* 0x05 cliff PSD readings */
    p[n++] = 0x05; p[n++] = 0x06;
    for (i = 0; i < 3; i++) n = put16(p, n, (rb.cliff & (1 << i)) ? 60 : 1800);

    /* 0x06 motor current, 10 mA units */
    p[n++] = 0x06; p[n++] = 0x02;
    p[n++] = (uint8_t)(5 + fabs(rb.vl) / 15.0);
    p[n++] = (uint8_t)(5 + fabs(rb.vr) / 15.0);

    /* 0x10 general purpose input */
    p[n++] = 0x10; p[n++] = 0x10;
    for (i = 0; i < 16; i++) p[n++] = 0;

    p[len_at] = (uint8_t)(n - 3);
    for (i = 2; i < n; i++) cs ^= p[i];
    p[n++] = cs;
    return n;
}
