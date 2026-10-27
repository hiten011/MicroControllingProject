#include "action_fault.h"
#include "fsm_states.h"

// Stub. ALIGN is not built yet: the AMU holds still until B1, a wheeldrop or a bumper sends it to FAULT.
// Read fsm_data.layout here to see what the Search learned.
static void align_hold_still(void)
{
    Kobuki_Drive(STOP);
}

static const FSM_Action align_hold_still_action = {
    .effect = align_hold_still,
};

static const FSM_Action *const align_actions[] = {
    &action_fault_ignore_cliff,
    &align_hold_still_action,
};

const FSM_State state_align = {
    .actions = align_actions,
    .action_count = FSM_ARRAY_LEN(align_actions),
};
