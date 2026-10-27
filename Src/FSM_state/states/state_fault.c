#include "fsm_states.h"

// The STOP / FAULT State of the diagram. Stop the motors at once, then go back to IDLE in the next Tick.
static void fault_init(void)
{
    Kobuki_Drive(STOP);
}

// No condition: always true.
static const FSM_Action fault_to_idle = {
    .next_state = FSM_ST_IDLE,
};

static const FSM_Action *const fault_actions[] = {
    &fault_to_idle,
};

const FSM_State state_fault = {
    .init = fault_init,
    .actions = fault_actions,
    .action_count = FSM_ARRAY_LEN(fault_actions),
};
