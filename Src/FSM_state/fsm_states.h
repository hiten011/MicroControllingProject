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

// Kobuki bumper / cliff bits: right, centre, left.
#define FSM_SIDE_RIGHT  0x01U
#define FSM_SIDE_CENTRE 0x02U
#define FSM_SIDE_LEFT   0x04U

// Search. The Spin Step has no value yet: the number below is a placeholder (TBD).
#define FSM_SPIN_STEP_DEG     15
#define FSM_SWEEP_DEG         360
#define FSM_SEARCH_MAX_SWEEPS 2

// Layout ranges in mm. Placeholders until measured on the hardware.
// Layout A: far bollard, SPLIT < range <= A_MAX. Layout B: near bollard, B_MIN <= range <= SPLIT.
#define FSM_LAYOUT_B_MIN_MM 300U
#define FSM_LAYOUT_SPLIT_MM 450U
#define FSM_LAYOUT_A_MAX_MM 650U

// Align: anticlockwise from first-detection heading to centre heading. 0 until measured.
#define FSM_ALIGNMENT_OFFSET_DEG 0

// Orient: Layout A turns CW to the bottom corridor; Layout B turns ACW to the left corridor.
#define FSM_ORIENT_A_DEG (-90)
#define FSM_ORIENT_B_DEG 90

// Pathfind. Tunable on hardware; values match docs/amu_fsm.drawio.
#define FSM_PATHFIND_DRIVE_SPEED 100
#define FSM_PATHFIND_WALL_A_DEG  (-10)  // CW off the storage wall
#define FSM_PATHFIND_WALL_B_DEG  10     // ACW off the storage wall
#define FSM_PATHFIND_LEDGE_A_DEG 10     // ACW off the ledge
#define FSM_PATHFIND_LEDGE_B_DEG (-10)  // CW off the ledge
#define FSM_PATHFIND_CORNER_A_DEG 90    // ACW onto the second corridor
#define FSM_PATHFIND_CORNER_B_DEG (-90) // CW onto the second corridor

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
    int16_t heading;        // Corridor heading (kobuki.angle ticks), latched by Orient / Front-cliff turn
    uint8_t corridor;       // 1 = first corridor, 2 = second; set by Orient / Pathfind
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
    FSM_ST_ALIGN,
    FSM_ST_ORIENT,
    FSM_ST_PATHFIND_DRIVE,
    FSM_ST_PATHFIND_WALL_HIT,
    FSM_ST_PATHFIND_LEDGE_HIT,
    FSM_ST_PATHFIND_REALIGN,
    FSM_ST_PATHFIND_FRONT_CLIFF_TURN,
    FSM_ST_PATHFIND_FRONT_CLIFF_STOP,
    FSM_ST_FAULT,
    FSM_ST_COUNT
} FSM_State_Id;

extern const FSM_State state_idle;
extern const FSM_State state_search;
extern const FSM_State state_detect;
extern const FSM_State state_spin;
extern const FSM_State state_align;
extern const FSM_State state_orient;
extern const FSM_State state_pathfind_drive;
extern const FSM_State state_pathfind_wall_hit;
extern const FSM_State state_pathfind_ledge_hit;
extern const FSM_State state_pathfind_realign;
extern const FSM_State state_pathfind_front_cliff_turn;
extern const FSM_State state_pathfind_front_cliff_stop;
extern const FSM_State state_fault;

extern const FSM_State *const fsm_states[FSM_ST_COUNT];

#endif
