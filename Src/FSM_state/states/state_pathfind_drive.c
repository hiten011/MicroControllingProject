#include "action_fault.h"
#include "fsm_states.h"

// Front centre cliff: end of the current corridor.
static bool front_cliff(void)
{
    return (fsm_data.kobuki.cliff & FSM_SIDE_CENTRE) != 0;
}

static bool front_cliff_corridor_1(void)
{
    return front_cliff() && fsm_data.corridor == 1;
}

static bool front_cliff_corridor_2(void)
{
    return front_cliff() && fsm_data.corridor == 2;
}

// Storage-wall bumper: Layout A left, Layout B right. Ledge-side bumper is ignored.
static bool storage_wall_bumper(void)
{
    uint8_t mask = (fsm_data.layout == FSM_LAYOUT_A) ? FSM_SIDE_LEFT : FSM_SIDE_RIGHT;
    return (fsm_data.kobuki.bumper & mask) != 0;
}

// Ledge cliff: Layout A right, Layout B left. Storage-wall-side cliff is ignored.
static bool ledge_cliff(void)
{
    uint8_t mask = (fsm_data.layout == FSM_LAYOUT_A) ? FSM_SIDE_RIGHT : FSM_SIDE_LEFT;
    return (fsm_data.kobuki.cliff & mask) != 0;
}

static void drive_forward(void)
{
    Kobuki_Drive(FSM_PATHFIND_DRIVE_SPEED);
}

static const FSM_Action drive_to_front_cliff_turn = {
    .condition = front_cliff_corridor_1,
    .next_state = FSM_ST_PATHFIND_FRONT_CLIFF_TURN,
};

static const FSM_Action drive_to_front_cliff_stop = {
    .condition = front_cliff_corridor_2,
    .next_state = FSM_ST_PATHFIND_FRONT_CLIFF_STOP,
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

// Priority matches the diagram: front cliff, then wall bumper, then ledge cliff.
static const FSM_Action *const drive_actions[] = {
    &action_fault_b1_wheeldrop,
    &drive_to_front_cliff_turn,
    &drive_to_front_cliff_stop,
    &drive_to_reverse_wall,
    &drive_to_reverse_ledge,
    &drive_on,
};

const FSM_State state_pathfind_drive = {
    .actions = drive_actions,
    .action_count = FSM_ARRAY_LEN(drive_actions),
};
