#include "action_fault.h"
#include "fsm_states.h"

// End of corridor 2: Pathfind is done. Hold until B1 / wheeldrop or Storage is built.
static void front_cliff_stop_init(void)
{
    Kobuki_Drive(STOP);
}

static void hold_still(void)
{
    Kobuki_Drive(STOP);
}

static const FSM_Action front_cliff_stop_hold = {
    .effect = hold_still,
};

static const FSM_Action *const front_cliff_stop_actions[] = {
    &action_fault_b1_wheeldrop,
    &front_cliff_stop_hold,
};

const FSM_State state_pathfind_front_cliff_stop = {
    .init = front_cliff_stop_init,
    .actions = front_cliff_stop_actions,
    .action_count = FSM_ARRAY_LEN(front_cliff_stop_actions),
};
