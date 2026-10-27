#include "action_fault.h"
#include "fsm_states.h"

// Turn a fixed angle away from the ledge: Layout A ACW, Layout B CW.
static void ledge_hit_init(void)
{
    int16_t deg =
        (fsm_data.layout == FSM_LAYOUT_A) ? FSM_PATHFIND_LEDGE_A_DEG : FSM_PATHFIND_LEDGE_B_DEG;
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, deg);
}

static bool rotation_complete(void)
{
    return fsm_data.kobuki.rotation_complete != 0;
}

static void ledge_hit_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static const FSM_Action ledge_hit_to_realign = {
    .condition = rotation_complete,
    .next_state = FSM_ST_PATHFIND_REALIGN,
};

static const FSM_Action ledge_hit_rotate_on = {
    .effect = ledge_hit_rotate,
};

static const FSM_Action *const ledge_hit_actions[] = {
    &action_fault_b1_wheeldrop,
    &ledge_hit_to_realign,
    &ledge_hit_rotate_on,
};

const FSM_State state_pathfind_ledge_hit = {
    .init = ledge_hit_init,
    .actions = ledge_hit_actions,
    .action_count = FSM_ARRAY_LEN(ledge_hit_actions),
};
