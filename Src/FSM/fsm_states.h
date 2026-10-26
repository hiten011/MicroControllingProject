#ifndef FSM_STATES_H
#define FSM_STATES_H

#include "fsm.h"

// State ids. They are the indexes of fsm_states[]. FSM_ST_FAULT is the STOP / FAULT State of the diagram,
// named FAULT so it does not clash with the STOP speed macro in kobuki.h.
typedef enum {
    FSM_ST_NONE = FSM_NO_STATE,
    FSM_ST_IDLE,
    FSM_ST_SEARCH,
    FSM_ST_DETECT,
    FSM_ST_SPIN,
    FSM_ST_ALIGN,  // Stub: holds the AMU still. Not built yet.
    FSM_ST_FAULT,
    FSM_ST_COUNT
} FSM_State_Id;

extern const FSM_State state_idle;
extern const FSM_State state_search;
extern const FSM_State state_detect;
extern const FSM_State state_spin;
extern const FSM_State state_align;
extern const FSM_State state_fault;

extern const FSM_State *const fsm_states[FSM_ST_COUNT];

#endif
