#include "action_fault.h"
#include "fsm_states.h"

// Storage-wall bumper fired: hand off to Realign (reverse, restore heading, resume).
static const FSM_Action wall_hit_to_realign = {
    .next_state = FSM_ST_PATHFIND_REALIGN,
};

static const FSM_Action *const wall_hit_actions[] = {
    &action_fault_b1_wheeldrop,
    &wall_hit_to_realign,
};

const FSM_State state_pathfind_wall_hit = {
    .actions = wall_hit_actions,
    .action_count = FSM_ARRAY_LEN(wall_hit_actions),
};
