#include "action_fault.h"
#include "fsm_states.h"

// Stub: stop inside the storage unit and end the run. Deposit-location sensing comes later.
static void storage_deposit_init(void)
{
    Kobuki_Drive(STOP);
}

static const FSM_Action storage_deposit_to_idle = {
    .next_state = FSM_ST_IDLE,
};

static const FSM_Action *const storage_deposit_actions[] = {
    &action_fault_b1_wheeldrop,
    &storage_deposit_to_idle,
};

const FSM_State state_storage_deposit = {
    .init = storage_deposit_init,
    .actions = storage_deposit_actions,
    .action_count = FSM_ARRAY_LEN(storage_deposit_actions),
};
