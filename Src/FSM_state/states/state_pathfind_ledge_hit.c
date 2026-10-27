#include "action_fault.h"
#include "fsm_states.h"

// Ledge-side cliff fired: hand off to Realign (reverse, restore heading, resume).
static const FSM_Action ledge_hit_to_realign = {
    .next_state = FSM_ST_PATHFIND_REALIGN,
};

static const FSM_Action *const ledge_hit_actions[] = {
    &action_fault_b1_wheeldrop,
    &ledge_hit_to_realign,
};

const FSM_State state_pathfind_ledge_hit = {
    .actions = ledge_hit_actions,
    .action_count = FSM_ARRAY_LEN(ledge_hit_actions),
};
