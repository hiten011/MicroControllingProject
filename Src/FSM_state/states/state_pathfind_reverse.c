#include "action_fault.h"
#include "fsm_states.h"

// Back off a fixed distance after a storage-wall bumper or ledge cliff during Drive.
static void reverse_init(void)
{
    Kobuki_Drive_Distance_Setpoint(fsm_data.kobuki.distance, -FSM_PATHFIND_REVERSE_MM);
}

static bool distance_complete(void)
{
    return fsm_data.kobuki.distance_complete != 0;
}

static void reverse_drive(void)
{
    Kobuki_Drive_Distance(fsm_data.kobuki.distance);
}

static const FSM_Action reverse_to_rotate = {
    .condition = distance_complete,
    .next_state = FSM_ST_PATHFIND_ROTATE,
};

static const FSM_Action reverse_on = {
    .effect = reverse_drive,
};

static const FSM_Action *const reverse_actions[] = {
    &action_fault_b1_wheeldrop,
    &reverse_to_rotate,
    &reverse_on,
};

const FSM_State state_pathfind_reverse = {
    .init = reverse_init,
    .actions = reverse_actions,
    .action_count = FSM_ARRAY_LEN(reverse_actions),
};
