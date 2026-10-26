/*
 * sim.h - shared declarations for the Kobuki / NUCLEO-G474RE simulator.
 *
 * World frame: millimetres, origin at the bottom-left inside corner of the
 * platform (Figure 2), +x to the right, +y up the page, angles CCW from +x.
 */
#ifndef SIM_H
#define SIM_H

#include <stdint.h>
#include <stdio.h>

#define SIM_PI          3.14159265358979323846
#define DEG(r)          ((r) * 180.0 / SIM_PI)
#define RAD(d)          ((d) * SIM_PI / 180.0)

/* ---- Kobuki --------------------------------------------------------- */
#define ROBOT_R         177.0       /* body radius, mm (354 mm diameter)    */
#define WHEEL_BASE      230.0       /* mm                                   */
#define SIM_MM_PER_TICK (2.0 * SIM_PI * 35.0 * (132.0 / 6545.0) / 52.0)
#define LOOP_MS         20          /* feedback packet period (50 Hz)       */
#define SPEED_LIMIT_MMS 150.0       /* brief: never exceed 150 mm/s         */

/* ---- Arena (Figure 2) ----------------------------------------------- */
#define PLAT_W          1600.0
#define PLAT_H          1800.0
#define BOLLARD_R       45.0
#define START_X         200.0
#define START_Y         200.0

typedef struct { double x0, y0, x1, y1; } Rect;

typedef enum { OBJ_BOLLARD, OBJ_MARKER } Obj_Kind;

typedef struct {
    const char *name;
    double      x, y, r;
    Obj_Kind    kind;
} Circle;

/* ---- Configuration -------------------------------------------------- */
#define MAX_TRIALS 8

typedef struct {
    char   layout;          /* 'A' or 'B'                                */
    int    deposit;         /* 1 or 2                                    */
    double heading_deg;     /* start heading in the world frame          */
} Trial_Spec;

typedef struct {
    Trial_Spec trials[MAX_TRIALS];
    int        n_trials;
    unsigned   seed;
    double     timeout_s;       /* per trial                              */
    double     b1_at_s;         /* <0 = off. Seconds after B0             */
    double     lift_at_s;       /* <0 = off. Seconds after B0             */
    double     lift_for_s;
    double     sensor_offset;   /* ultrasonic, mm in front of centre      */
    double     beam_half_deg;   /* ultrasonic half-angle for a bollard    */
    double     us_noise_mm;
    double     us_latency_ms;
    double     gyro_drift_dpm;  /* deg per minute                         */
    double     accel;           /* wheel acceleration limit, mm/s^2       */
    double     start_jitter;    /* mm                                     */
    double     gap_tol, align_tol;
    int        strict;
    int        html;
    int        quiet;
    char       json_path[512];
    char       out_dir[512];
    char       tag[128];        /* output file name stem                  */
    char       exe_dir[512];
} Sim_Config;

extern Sim_Config cfg;

/* ---- Robot state (owned by sim_robot.c) ----------------------------- */
typedef struct {
    double   x, y, th;          /* true pose                              */
    double   vl, vr;            /* actual wheel speeds, mm/s              */
    double   tl, tr;            /* target wheel speeds from the command   */
    int16_t  cmd_speed, cmd_radius;
    double   enc_l, enc_r;      /* encoder ticks (fractional)             */
    double   gyro_zero;         /* rad: world heading where gyro reads 0  */
    uint8_t  bumper, cliff, wheeldrop, button;
    int      lifted, fallen;
    uint16_t gpo;               /* LED / power outputs                    */
    uint16_t us_range;          /* ultrasonic analogue output, mm         */
    uint16_t us_adc;            /* last ADC code read                     */
    uint32_t t_ms;
} Robot;

extern Robot rb;

/* ---- sim_mmio.c ----------------------------------------------------- */
int  mmio_init(int trap);
void mmio_unprotect_all(void);

/* ---- sim_periph.c --------------------------------------------------- */
void periph_reset(void);
void periph_before_read(uintptr_t reg);
void periph_after_read(uintptr_t reg);
void periph_after_write(uintptr_t reg, uint32_t old_v, uint32_t new_v);
int  periph_check_config(char *summary, size_t n);

/* ---- sim_robot.c ---------------------------------------------------- */
void        sim_seed(unsigned s);
double      sim_rand(void);                   /* [0, 1)                   */
void        world_setup_trial(const Trial_Spec *t, int first);
void        world_tick(void);                 /* advance LOOP_MS          */
void        kobuki_rx_byte(uint8_t b);        /* MCU -> Kobuki            */
int         kobuki_feedback(uint8_t *buf);    /* Kobuki -> MCU packet     */
uint16_t    sensor_adc(int channel);
int         world_objects(const Circle **list);
const Rect *world_walls(int *n);
int         section_of(double x, double y);
int         robot_in_storage(double *margin);
const Circle *active_marker(void);
double      wheel_speed_cmd_max(void);

/* ---- sim_main.c ----------------------------------------------------- */
void sim_feed_needed(void);                   /* firmware is waiting for data */
void sim_warn(const char *key, const char *fmt, ...);
void sim_event(const char *kind, const char *fmt, ...);
void sim_on_command(int16_t speed, int16_t radius, double wl, double wr);
void sim_on_contact(const char *name, Obj_Kind kind);
void sim_on_fall(void);
void sim_fatal(const char *fmt, ...);

#endif /* SIM_H */
