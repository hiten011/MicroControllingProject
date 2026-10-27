#include "action_fault.h"
#include "fsm_states.h"

// End of corridor 2: Pathfind is done. Stop once, then ε into Storage Turn.
static void corridor_stop_init(void)
{
    Kobuki_Drive(STOP);
}

static const FSM_Action corridor_stop_to_storage_turn = {
    .next_state = FSM_ST_STORAGE_TURN,
};

static const FSM_Action *const corridor_stop_actions[] = {
    &action_fault_b1_wheeldrop,
    &corridor_stop_to_storage_turn,
};

const FSM_State state_pathfind_corridor_stop = {
    .init = corridor_stop_init,
    .actions = corridor_stop_actions,
    .action_count = FSM_ARRAY_LEN(corridor_stop_actions),
};
