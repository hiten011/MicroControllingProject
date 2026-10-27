#include "action_fault.h"
#include "fsm_states.h"

// Corridor length reached: net forward mm from corridor start (Reverse subtracts).
static bool corridor_length_reached(void)
{
    return Kobuki_Distance_Delta_Mm(fsm_data.corridor_start_distance, fsm_data.kobuki.distance) >=
           FSM_PATHFIND_CORRIDOR_MM;
}

static bool corridor_1_done(void)
{
    return corridor_length_reached() && fsm_data.corridor == 1;
}

static bool corridor_2_done(void)
{
    return corridor_length_reached() && fsm_data.corridor == 2;
}

// Storage-wall bumper: Layout A right, Layout B left. Ledge-side bumper is ignored.
static bool storage_wall_bumper(void)
{
    uint8_t mask = (fsm_data.layout == FSM_LAYOUT_A) ? FSM_SIDE_RIGHT : FSM_SIDE_LEFT;
    return (fsm_data.kobuki.bumper & mask) != 0;
}

// Ledge cliff: Layout A left, Layout B right. Storage-wall-side cliff is ignored.
static bool ledge_cliff(void)
{
    uint8_t mask = (fsm_data.layout == FSM_LAYOUT_A) ? FSM_SIDE_LEFT : FSM_SIDE_RIGHT;
    return (fsm_data.kobuki.cliff & mask) != 0;
}

static void drive_forward(void)
{
    Kobuki_Drive(FSM_PATHFIND_DRIVE_SPEED);
}

static const FSM_Action drive_to_corridor_turn = {
    .condition = corridor_1_done,
    .next_state = FSM_ST_PATHFIND_CORRIDOR_TURN,
};

static const FSM_Action drive_to_corridor_stop = {
    .condition = corridor_2_done,
    .next_state = FSM_ST_PATHFIND_CORRIDOR_STOP,
};

static const FSM_Action drive_to_reverse_wall = {
    .condition = storage_wall_bumper,
    .next_state = FSM_ST_PATHFIND_REVERSE,
};

static const FSM_Action drive_to_reverse_ledge = {
    .condition = ledge_cliff,
    .next_state = FSM_ST_PATHFIND_REVERSE,
};

// No condition: the do / of the State, runs when no Action above it ran.
static const FSM_Action drive_on = {
    .effect = drive_forward,
};

// Priority: corridor length, then wall bumper, then ledge cliff. Centre cliff ignored.
static const FSM_Action *const drive_actions[] = {
    &action_fault_b1_wheeldrop,
    &drive_to_corridor_turn,
    &drive_to_corridor_stop,
    &drive_to_reverse_wall,
    &drive_to_reverse_ledge,
    &drive_on,
};

const FSM_State state_pathfind_drive = {
    .actions = drive_actions,
    .action_count = FSM_ARRAY_LEN(drive_actions),
};
