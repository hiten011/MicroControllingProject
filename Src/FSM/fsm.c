#include "fsm.h"

static const FSM_State *const *state_table = NULL;
static size_t state_count = 0;
static int current_state = FSM_NO_STATE;

static bool State_Exists(int id)
{
    return state_table != NULL && id > FSM_NO_STATE && (size_t)id < state_count && state_table[id] != NULL;
}

static void State_Enter(int id)
{
    current_state = id;

    if (state_table[id]->init != NULL) {
        state_table[id]->init();
    }
}

static void State_Leave(void)
{
    if (State_Exists(current_state) && state_table[current_state]->exit != NULL) {
        state_table[current_state]->exit();
    }
}

static void Action_Fire(const FSM_Action *action)
{
    // An unknown next_state counts as no change: only the effect runs.
    bool change_state = State_Exists(action->next_state);

    if (change_state) {
        State_Leave();
    }

    if (action->effect != NULL) {
        action->effect();
    }

    if (change_state) {
        State_Enter(action->next_state);
    }
}

// Run the first Action with a true condition and skip the rest.
static void State_Run(const FSM_State *state)
{
    if (state->actions == NULL) {
        return;
    }

    for (size_t i = 0; i < state->action_count; i++) {
        const FSM_Action *action = state->actions[i];

        if (action == NULL) {
            continue;
        }

        if (action->condition == NULL || action->condition()) {
            Action_Fire(action);
            return;
        }
    }
}

bool FSM_Init(const FSM_State *const *states, size_t count, int start_state)
{
    state_table = states;
    state_count = count;
    current_state = FSM_NO_STATE;

    if (!State_Exists(start_state)) {
        return false;
    }

    State_Enter(start_state);
    return true;
}

void FSM_Tick(void)
{
    if (State_Exists(current_state)) {
        State_Run(state_table[current_state]);
    }
}

bool FSM_Change_State(int next_state)
{
    if (!State_Exists(next_state)) {
        return false;
    }

    State_Leave();
    State_Enter(next_state);
    return true;
}

int FSM_Current_State(void)
{
    return current_state;
}
