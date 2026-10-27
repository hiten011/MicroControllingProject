#include "action_fault.h"
#include "fsm_states.h"

// End of corridor 1: turn onto corridor 2. Layout A ACW, Layout B CW.
static void front_cliff_turn_init(void)
{
    int16_t deg = (fsm_data.layout == FSM_LAYOUT_A) ? FSM_PATHFIND_CORNER_A_DEG
                                                    : FSM_PATHFIND_CORNER_B_DEG;
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, deg);
}

static void front_cliff_turn_exit(void)
{
    fsm_data.heading = fsm_data.kobuki.angle;
}

static bool rotation_complete(void)
{
    return fsm_data.kobuki.rotation_complete != 0;
}

static void start_second_corridor(void)
{
    fsm_data.corridor = 2;
}

static void front_cliff_turn_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static const FSM_Action front_cliff_turn_to_drive = {
    .condition = rotation_complete,
    .effect = start_second_corridor,
    .next_state = FSM_ST_PATHFIND_DRIVE,
};

static const FSM_Action front_cliff_turn_rotate_on = {
    .effect = front_cliff_turn_rotate,
};

static const FSM_Action *const front_cliff_turn_actions[] = {
    &action_fault_b1_wheeldrop,
    &front_cliff_turn_to_drive,
    &front_cliff_turn_rotate_on,
};

const FSM_State state_pathfind_front_cliff_turn = {
    .init = front_cliff_turn_init,
    .exit = front_cliff_turn_exit,
    .actions = front_cliff_turn_actions,
    .action_count = FSM_ARRAY_LEN(front_cliff_turn_actions),
};
