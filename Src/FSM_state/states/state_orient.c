#include "action_fault.h"
#include "fsm_states.h"

// Face the first corridor: Layout A turns CW (-90), Layout B turns ACW (+90).
static void orient_init(void)
{
    int16_t deg = (fsm_data.layout == FSM_LAYOUT_A) ? FSM_ORIENT_A_DEG : FSM_ORIENT_B_DEG;
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, deg);
}

static bool rotation_complete(void)
{
    return fsm_data.kobuki.rotation_complete != 0;
}

static void latch_corridor_heading(void)
{
    fsm_data.heading = fsm_data.kobuki.angle;
    fsm_data.corridor = 1;
    fsm_data.corridor_start_distance = fsm_data.kobuki.distance;
}

static void orient_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static const FSM_Action orient_to_pathfind = {
    .condition = rotation_complete,
    .effect = latch_corridor_heading,
    .next_state = FSM_ST_PATHFIND_DRIVE,
};

// No condition: the do / of the State, runs when no Action above it ran.
static const FSM_Action orient_rotate_on = {
    .effect = orient_rotate,
};

static const FSM_Action *const orient_actions[] = {
    &action_fault_all,
    &orient_to_pathfind,
    &orient_rotate_on,
};

const FSM_State state_orient = {
    .init = orient_init,
    .actions = orient_actions,
    .action_count = FSM_ARRAY_LEN(orient_actions),
};
