#include "action_fault.h"
#include "fsm_config.h"
#include "fsm_data.h"
#include "fsm_states.h"

static bool stop_button_or_wheeldrop(void)
{
    return (fsm_data.kobuki.button & FSM_BUTTON_B1) != 0 || fsm_data.kobuki.wheeldrop != 0;
}

static bool fault_all(void)
{
    return stop_button_or_wheeldrop() || fsm_data.kobuki.bumper != 0 || fsm_data.kobuki.cliff != 0;
}

static bool fault_ignore_cliff(void)
{
    return stop_button_or_wheeldrop() || fsm_data.kobuki.bumper != 0;
}

const FSM_Action action_fault_all = {
    .condition = fault_all,
    .next_state = FSM_ST_FAULT,
};

const FSM_Action action_fault_ignore_cliff = {
    .condition = fault_ignore_cliff,
    .next_state = FSM_ST_FAULT,
};
