#include "fsm_states.h"

// Forget the last Search, because SEARCH cannot reset itself: DETECT and SPIN re-enter it.
static void idle_exit(void)
{
    fsm_data.layout = FSM_LAYOUT_NONE;
    fsm_data.object.detected = 0;
    fsm_data.swept_deg = 0;
    fsm_data.sweep_count = 0;
    fsm_data.last_angle = fsm_data.kobuki.angle;
}

static bool start_button_pressed(void)
{
    return (fsm_data.kobuki.button & FSM_BUTTON_B0) != 0;
}

static void hold_still(void)
{
    Kobuki_Drive(STOP);
}

static const FSM_Action idle_to_search = {
    .condition = start_button_pressed,
    .next_state = FSM_ST_SEARCH,
};

// No condition: the do / of the State, runs when no Action above it ran.
static const FSM_Action idle_hold_still = {
    .effect = hold_still,
};

static const FSM_Action *const idle_actions[] = {
    &idle_to_search,
    &idle_hold_still,
};

const FSM_State state_idle = {
    .exit = idle_exit,
    .actions = idle_actions,
    .action_count = FSM_ARRAY_LEN(idle_actions),
};
