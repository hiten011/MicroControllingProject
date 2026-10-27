#include "action_fault.h"
#include "fsm_states.h"
#include "helper.h"

// Restore corridor heading after Reverse, then return to Drive.
static void rotate_init(void)
{
    int16_t deg = Angle_Delta_Deg(fsm_data.kobuki.angle, fsm_data.heading);
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, deg);
}

static bool rotation_complete(void)
{
    return fsm_data.kobuki.rotation_complete != 0;
}

static void pathfind_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static const FSM_Action rotate_to_drive = {
    .condition = rotation_complete,
    .next_state = FSM_ST_PATHFIND_DRIVE,
};

static const FSM_Action rotate_on = {
    .effect = pathfind_rotate,
};

static const FSM_Action *const rotate_actions[] = {
    &action_fault_b1_wheeldrop,
    &rotate_to_drive,
    &rotate_on,
};

const FSM_State state_pathfind_rotate = {
    .init = rotate_init,
    .actions = rotate_actions,
    .action_count = FSM_ARRAY_LEN(rotate_actions),
};
