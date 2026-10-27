#include "action_fault.h"
#include "fsm_states.h"
#include "helper.h"

// Read the last Detect reading. This is in init() and not in a do / Action, because the Actions below
// are checked before any do / Action and they need the Layout. It is overwritten at every Detection.
static void search_init(void)
{
    if (fsm_data.object.detected) {
        fsm_data.layout = Layout_From_Range(fsm_data.object.range_mm);
    }
}

static bool layout_learned(void)
{
    return fsm_data.object.detected && fsm_data.layout != FSM_LAYOUT_NONE;
}

// A Detection that fits neither Layout, or the Search used all its Sweeps.
static bool search_gave_up(void)
{
    return (fsm_data.object.detected && fsm_data.layout == FSM_LAYOUT_NONE) ||
           fsm_data.sweep_count >= FSM_SEARCH_MAX_SWEEPS;
}

static bool nothing_detected_yet(void)
{
    return !fsm_data.object.detected && fsm_data.sweep_count < FSM_SEARCH_MAX_SWEEPS;
}

static const FSM_Action search_to_align = {
    .condition = layout_learned,
    .next_state = FSM_ST_ALIGN,
};

static const FSM_Action search_to_fault = {
    .condition = search_gave_up,
    .next_state = FSM_ST_FAULT,
};

static const FSM_Action search_to_detect = {
    .condition = nothing_detected_yet,
    .next_state = FSM_ST_DETECT,
};

static const FSM_Action *const search_actions[] = {
    &action_fault_all,
    &search_to_align,
    &search_to_fault,
    &search_to_detect,
};

const FSM_State state_search = {
    .init = search_init,
    .actions = search_actions,
    .action_count = FSM_ARRAY_LEN(search_actions),
};
