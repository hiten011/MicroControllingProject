#include "action_fault.h"
#include "fsm_states.h"
#include "helper.h"

// Shortest signed turn back to the corridor heading after a Wall hit or Ledge hit.
static void realign_init(void)
{
    int16_t deg = Angle_Delta_Deg(fsm_data.kobuki.angle, fsm_data.heading);
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, deg);
}

static bool rotation_complete(void)
{
    return fsm_data.kobuki.rotation_complete != 0;
}

static void realign_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static const FSM_Action realign_to_drive = {
    .condition = rotation_complete,
    .next_state = FSM_ST_PATHFIND_DRIVE,
};

static const FSM_Action realign_rotate_on = {
    .effect = realign_rotate,
};

static const FSM_Action *const realign_actions[] = {
    &action_fault_b1_wheeldrop,
    &realign_to_drive,
    &realign_rotate_on,
};

const FSM_State state_pathfind_realign = {
    .init = realign_init,
    .actions = realign_actions,
    .action_count = FSM_ARRAY_LEN(realign_actions),
};
