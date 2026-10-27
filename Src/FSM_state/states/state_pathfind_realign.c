#include "action_fault.h"
#include "fsm_states.h"
#include "helper.h"

// Reverse clear of the obstacle, rotate to corridor heading, then drive forward again.
typedef enum {
    REALIGN_PHASE_REVERSE,
    REALIGN_PHASE_ROTATE,
    REALIGN_PHASE_FORWARD,
} Realign_Phase;

static Realign_Phase phase;

static void realign_init(void)
{
    phase = REALIGN_PHASE_REVERSE;
    Kobuki_Drive_Distance_Setpoint(fsm_data.kobuki.distance, -FSM_PATHFIND_REALIGN_REVERSE_MM);
}

static bool reverse_incomplete(void)
{
    return phase == REALIGN_PHASE_REVERSE && fsm_data.kobuki.distance_complete == 0;
}

static bool need_arm_rotate(void)
{
    return phase == REALIGN_PHASE_REVERSE && fsm_data.kobuki.distance_complete != 0;
}

static bool rotate_incomplete(void)
{
    return phase == REALIGN_PHASE_ROTATE && fsm_data.kobuki.rotation_complete == 0;
}

static bool need_arm_forward(void)
{
    return phase == REALIGN_PHASE_ROTATE && fsm_data.kobuki.rotation_complete != 0;
}

static bool forward_incomplete(void)
{
    return phase == REALIGN_PHASE_FORWARD && fsm_data.kobuki.distance_complete == 0;
}

static bool forward_complete(void)
{
    return phase == REALIGN_PHASE_FORWARD && fsm_data.kobuki.distance_complete != 0;
}

static void realign_reverse(void)
{
    Kobuki_Drive_Distance(fsm_data.kobuki.distance);
}

static void realign_arm_rotate(void)
{
    int16_t deg = Angle_Delta_Deg(fsm_data.kobuki.angle, fsm_data.heading);
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, deg);
    phase = REALIGN_PHASE_ROTATE;
}

static void realign_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static void realign_arm_forward(void)
{
    Kobuki_Drive_Distance_Setpoint(fsm_data.kobuki.distance, FSM_PATHFIND_REALIGN_REVERSE_MM);
    phase = REALIGN_PHASE_FORWARD;
}

static void realign_forward(void)
{
    Kobuki_Drive_Distance(fsm_data.kobuki.distance);
}

static const FSM_Action realign_reverse_on = {
    .condition = reverse_incomplete,
    .effect = realign_reverse,
};

static const FSM_Action realign_arm_rotate_on = {
    .condition = need_arm_rotate,
    .effect = realign_arm_rotate,
};

static const FSM_Action realign_rotate_on = {
    .condition = rotate_incomplete,
    .effect = realign_rotate,
};

static const FSM_Action realign_arm_forward_on = {
    .condition = need_arm_forward,
    .effect = realign_arm_forward,
};

static const FSM_Action realign_forward_on = {
    .condition = forward_incomplete,
    .effect = realign_forward,
};

static const FSM_Action realign_to_drive = {
    .condition = forward_complete,
    .next_state = FSM_ST_PATHFIND_DRIVE,
};

static const FSM_Action *const realign_actions[] = {
    &action_fault_b1_wheeldrop,
    &realign_reverse_on,
    &realign_arm_rotate_on,
    &realign_rotate_on,
    &realign_arm_forward_on,
    &realign_forward_on,
    &realign_to_drive,
};

const FSM_State state_pathfind_realign = {
    .init = realign_init,
    .actions = realign_actions,
    .action_count = FSM_ARRAY_LEN(realign_actions),
};
