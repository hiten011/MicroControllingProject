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
    [FSM_ST_PATHFIND_DRIVE] = &state_pathfind_drive,
    [FSM_ST_PATHFIND_REVERSE] = &state_pathfind_reverse,
    [FSM_ST_PATHFIND_ROTATE] = &state_pathfind_rotate,
    [FSM_ST_PATHFIND_CORRIDOR_TURN] = &state_pathfind_corridor_turn,
    [FSM_ST_PATHFIND_CORRIDOR_STOP] = &state_pathfind_corridor_stop,
    [FSM_ST_STORAGE_TURN] = &state_storage_turn,
    [FSM_ST_STORAGE_DRIVE_IN] = &state_storage_drive_in,
    [FSM_ST_STORAGE_DEPOSIT] = &state_storage_deposit,
    [FSM_ST_FAULT] = &state_fault,
};
