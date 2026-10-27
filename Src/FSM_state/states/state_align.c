#include "action_fault.h"
#include "fsm_states.h"

// Turn from the first-detection heading to the centre heading (anticlockwise by the Alignment offset).
static void align_init(void)
{
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, FSM_ALIGNMENT_OFFSET_DEG);
}

static bool rotation_complete(void)
{
    return fsm_data.kobuki.rotation_complete != 0;
}

static void align_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static const FSM_Action align_to_orient = {
    .condition = rotation_complete,
    .next_state = FSM_ST_ORIENT,
};

// No condition: the do / of the State, runs when no Action above it ran.
static const FSM_Action align_rotate_on = {
    .effect = align_rotate,
};

static const FSM_Action *const align_actions[] = {
    &action_fault_ignore_cliff,
    &align_to_orient,
    &align_rotate_on,
};

const FSM_State state_align = {
    .init = align_init,
    .actions = align_actions,
    .action_count = FSM_ARRAY_LEN(align_actions),
};
