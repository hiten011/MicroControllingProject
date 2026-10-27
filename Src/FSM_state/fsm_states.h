#ifndef FSM_STATES_H
#define FSM_STATES_H

#include <stdint.h>
#include "fsm.h"
#include "kobuki.h"
#include "ultrasonic.h"

// --- Config ---

// Kobuki button bits: B0 = start, B1 = stop.
#define FSM_BUTTON_B0 0x01U
#define FSM_BUTTON_B1 0x02U

// Search. The Spin Step has no value yet: the number below is a placeholder (TBD).
#define FSM_SPIN_STEP_DEG     15
#define FSM_SWEEP_DEG         360
#define FSM_SEARCH_MAX_SWEEPS 2

// Layout ranges in mm. Placeholders until measured on the hardware.
// Layout A: far bollard, SPLIT < range <= A_MAX. Layout B: near bollard, B_MIN <= range <= SPLIT.
#define FSM_LAYOUT_B_MIN_MM 300U
#define FSM_LAYOUT_SPLIT_MM 450U
#define FSM_LAYOUT_A_MAX_MM 650U

// Kobuki angle: one tick is 0.01 degree and the reading wraps at +/-180 degrees.
#define FSM_ANGLE_TICKS_PER_DEG   100
#define FSM_ANGLE_TICKS_HALF_TURN 18000
#define FSM_ANGLE_TICKS_FULL_TURN 36000

// --- Shared data ---

typedef enum { FSM_LAYOUT_NONE, FSM_LAYOUT_A, FSM_LAYOUT_B } FSM_Layout;

// The values the conditions, effects, init() and exit() functions of the States read and write.
typedef struct {
    Kobuki_Typedef kobuki;  // Sensors, read at the top of every Tick with Kobuki_Read()
    US_Typedef object;      // Last Detect reading
    FSM_Layout layout;      // Learned by the Search
    int16_t swept_deg;      // Angle spun in the current Sweep, + = anticlockwise
    uint8_t sweep_count;    // Sweeps finished without a Detection
    int16_t last_angle;     // kobuki.angle (ticks) at the last Spin exit
} FSM_Data;

extern FSM_Data fsm_data;

// --- State ids and table ---

// State ids. They are the indexes of fsm_states[]. FSM_ST_FAULT is the STOP / FAULT State of the diagram,
// named FAULT so it does not clash with the STOP speed macro in kobuki.h.
typedef enum {
    FSM_ST_NONE = FSM_NO_STATE,
    FSM_ST_IDLE,
    FSM_ST_SEARCH,
    FSM_ST_DETECT,
    FSM_ST_SPIN,
    FSM_ST_ALIGN,  // Stub: holds the AMU still. Not built yet.
    FSM_ST_FAULT,
    FSM_ST_COUNT
} FSM_State_Id;

extern const FSM_State state_idle;
extern const FSM_State state_search;
extern const FSM_State state_detect;
extern const FSM_State state_spin;
extern const FSM_State state_align;
extern const FSM_State state_fault;

extern const FSM_State *const fsm_states[FSM_ST_COUNT];

#endif
