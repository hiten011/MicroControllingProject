#include "action_fault.h"
#include "fsm_states.h"

// Face the Entrance: Layout A turns ACW (+135), Layout B turns CW (-135).
static void storage_turn_init(void)
{
    int16_t deg =
        (fsm_data.layout == FSM_LAYOUT_A) ? FSM_STORAGE_TURN_A_DEG : FSM_STORAGE_TURN_B_DEG;
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, deg);
}

static bool rotation_complete(void)
{
    return fsm_data.kobuki.rotation_complete != 0;
}

static void storage_turn_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static const FSM_Action storage_turn_to_drive_in = {
    .condition = rotation_complete,
    .next_state = FSM_ST_STORAGE_DRIVE_IN,
};

static const FSM_Action storage_turn_rotate_on = {
    .effect = storage_turn_rotate,
};

static const FSM_Action *const storage_turn_actions[] = {
    &action_fault_b1_wheeldrop,
    &storage_turn_to_drive_in,
    &storage_turn_rotate_on,
};

const FSM_State state_storage_turn = {
    .init = storage_turn_init,
    .actions = storage_turn_actions,
    .action_count = FSM_ARRAY_LEN(storage_turn_actions),
};
