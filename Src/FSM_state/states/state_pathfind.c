#include "action_fault.h"
#include "fsm_states.h"

// Stub. PATHFIND is not built yet: the AMU holds still until B1 or a wheeldrop sends it to FAULT.
static void pathfind_hold_still(void)
{
    Kobuki_Drive(STOP);
}

static const FSM_Action pathfind_hold_still_action = {
    .effect = pathfind_hold_still,
};

static const FSM_Action *const pathfind_actions[] = {
    &action_fault_b1_wheeldrop,
    &pathfind_hold_still_action,
};

const FSM_State state_pathfind = {
    .actions = pathfind_actions,
    .action_count = FSM_ARRAY_LEN(pathfind_actions),
};
