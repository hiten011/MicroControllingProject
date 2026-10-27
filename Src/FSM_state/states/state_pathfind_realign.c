#include "action_fault.h"
#include "fsm_states.h"
#include "helper.h"

// Fixed reverse, then shortest signed turn back to the corridor heading.
static bool rotate_armed;

static void realign_init(void)
{
    rotate_armed = false;
    Kobuki_Drive_Distance_Setpoint(fsm_data.kobuki.distance, -FSM_PATHFIND_REALIGN_REVERSE_MM);
}

static bool reverse_incomplete(void)
{
    return fsm_data.kobuki.distance_complete == 0;
}

static bool need_arm_rotate(void)
{
    return fsm_data.kobuki.distance_complete != 0 && !rotate_armed;
}

static bool rotation_complete(void)
{
    return fsm_data.kobuki.distance_complete != 0 && rotate_armed &&
           fsm_data.kobuki.rotation_complete != 0;
}

static bool rotate_incomplete(void)
{
    return fsm_data.kobuki.distance_complete != 0 && rotate_armed &&
           fsm_data.kobuki.rotation_complete == 0;
}

static void realign_reverse(void)
{
    Kobuki_Drive_Distance(fsm_data.kobuki.distance);
}

static void realign_arm_rotate(void)
{
    int16_t deg = Angle_Delta_Deg(fsm_data.kobuki.angle, fsm_data.heading);
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, deg);
    rotate_armed = true;
}

static void realign_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static const FSM_Action realign_reverse_on = {
    .condition = reverse_incomplete,
    .effect = realign_reverse,
};

static const FSM_Action realign_arm_rotate_on = {
    .condition = need_arm_rotate,
    .effect = realign_arm_rotate,
};

static const FSM_Action realign_to_drive = {
    .condition = rotation_complete,
    .next_state = FSM_ST_PATHFIND_DRIVE,
};

static const FSM_Action realign_rotate_on = {
    .condition = rotate_incomplete,
    .effect = realign_rotate,
};

static const FSM_Action *const realign_actions[] = {
    &action_fault_b1_wheeldrop,
    &realign_reverse_on,
    &realign_arm_rotate_on,
    &realign_to_drive,
    &realign_rotate_on,
};

const FSM_State state_pathfind_realign = {
    .init = realign_init,
    .actions = realign_actions,
    .action_count = FSM_ARRAY_LEN(realign_actions),
};
