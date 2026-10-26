#include "action_fault.h"
#include "fsm_data.h"
#include "fsm_states.h"

// Hold still and take one Detect reading. This is in init() and not in a do / Action, because the
// Actions below are checked before any do / Action and they need the reading.
static void detect_init(void)
{
    Kobuki_Drive(STOP);
    Ultrasonic_Detect(&fsm_data.object);
}

static bool detection(void)
{
    return fsm_data.object.detected;
}

static bool no_detection(void)
{
    return !fsm_data.object.detected;
}

static const FSM_Action detect_to_search = {
    .condition = detection,
    .next_state = FSM_ST_SEARCH,
};

static const FSM_Action detect_to_spin = {
    .condition = no_detection,
    .next_state = FSM_ST_SPIN,
};

static const FSM_Action *const detect_actions[] = {
    &action_fault_all,
    &detect_to_search,
    &detect_to_spin,
};

const FSM_State state_detect = {
    .init = detect_init,
    .actions = detect_actions,
    .action_count = FSM_ARRAY_LEN(detect_actions),
};
