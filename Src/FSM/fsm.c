#include "fsm.h"

static const FSM_State *const *state_table = NULL;
static int current_state = FSM_NO_STATE;

static void State_Enter(int id)
{
    current_state = id;

    if (state_table[id]->init != NULL) {
        state_table[id]->init();
    }
}

static void State_Leave(void)
{
    if (current_state != FSM_NO_STATE && state_table[current_state]->exit != NULL) {
        state_table[current_state]->exit();
    }
}

static void Action_Fire(const FSM_Action *action)
{
    // FSM_NO_STATE means stay: only the effect runs.
    bool change_state = action->next_state != FSM_NO_STATE;

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

bool FSM_Init(const FSM_State *const *states, size_t state_count, int start_state)
{
    (void)state_count;

    state_table = states;
    current_state = FSM_NO_STATE;
    State_Enter(start_state);
    return true;
}

void FSM_Tick(void)
{
    if (current_state != FSM_NO_STATE) {
        State_Run(state_table[current_state]);
    }
}

bool FSM_Change_State(int next_state)
{
    State_Leave();
    State_Enter(next_state);
    return true;
}

int FSM_Current_State(void)
{
    return current_state;
}
