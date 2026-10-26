#ifndef FSM_H
#define FSM_H

#include <stdbool.h>
#include <stddef.h>

// State id 0 is reserved: it means "no state". An Action that leaves next_state out stays in its State,
// and a state table leaves index 0 NULL.
#define FSM_NO_STATE 0

#define FSM_ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

// An Action is a condition and what to do when it holds.
typedef struct {
    // NULL means the condition is always true.
    bool (*condition)(void);
    // Runs when the condition holds. NULL means nothing to run.
    void (*effect)(void);
    // The State to move to after the effect. FSM_NO_STATE means stay in the State.
    int next_state;
} FSM_Action;

// A State holds its Actions in order of priority.
typedef struct {
    // Runs once when the State is entered. NULL means nothing to run.
    void (*init)(void);
    // Runs once when the State is left. NULL means nothing to run.
    void (*exit)(void);
    const FSM_Action *const *actions;
    size_t action_count;
} FSM_State;

// Make start_state the current State and run its init(). The table is indexed by state id.
// Returns false, and enters no State, if the table has no State at start_state.
bool FSM_Init(const FSM_State *const *states, size_t state_count, int start_state);

// Check the Actions of the current State in order. The first Action with a true condition runs
// and the rest are skipped, so one Tick runs at most one Action.
// When that Action has a next_state, the order is: exit() of the old State, the effect,
// init() of the new State. The Actions of the new State are first checked in the next Tick.
// Call once per received Kobuki packet, after Kobuki_Read().
void FSM_Tick(void);

// Leave the current State (exit()) and enter next_state (init()).
// Returns false, and changes nothing, if the table has no State at next_state.
bool FSM_Change_State(int next_state);

// The id of the current State, or FSM_NO_STATE before FSM_Init() succeeds.
int FSM_Current_State(void);

#endif
