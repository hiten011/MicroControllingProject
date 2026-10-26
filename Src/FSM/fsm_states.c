#include "fsm_states.h"

const FSM_State *const fsm_states[FSM_ST_COUNT] = {
    [FSM_ST_IDLE] = &state_idle, [FSM_ST_SEARCH] = &state_search, [FSM_ST_DETECT] = &state_detect,
    [FSM_ST_SPIN] = &state_spin, [FSM_ST_ALIGN] = &state_align,   [FSM_ST_FAULT] = &state_fault,
};
