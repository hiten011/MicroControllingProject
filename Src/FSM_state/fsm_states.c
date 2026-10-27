#include "fsm_states.h"

// Static storage: every field starts at 0, so the Layout starts as FSM_LAYOUT_NONE.
FSM_Data fsm_data;

const FSM_State *const fsm_states[FSM_ST_COUNT] = {
    [FSM_ST_IDLE] = &state_idle,
    [FSM_ST_SEARCH] = &state_search,
    [FSM_ST_DETECT] = &state_detect,
    [FSM_ST_SPIN] = &state_spin,
    [FSM_ST_ALIGN] = &state_align,
    [FSM_ST_ORIENT] = &state_orient,
    [FSM_ST_PATHFIND] = &state_pathfind,
    [FSM_ST_FAULT] = &state_fault,
};
