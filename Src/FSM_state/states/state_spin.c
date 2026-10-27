#include "action_fault.h"
#include "fsm_states.h"
#include "helper.h"

// One Step, anticlockwise.
static void spin_init(void)
{
    Kobuki_Rotate_Angle_Setpoint(fsm_data.kobuki.angle, FSM_SPIN_STEP_DEG);
}

// Add the angle of this Step to the Sweep. A full 360 degrees is one finished Sweep.
static void spin_exit(void)
{
    fsm_data.swept_deg += Angle_Delta_Deg(fsm_data.last_angle, fsm_data.kobuki.angle);
    fsm_data.last_angle = fsm_data.kobuki.angle;

    if (fsm_data.swept_deg >= FSM_SWEEP_DEG) {
        fsm_data.swept_deg -= FSM_SWEEP_DEG;
        fsm_data.sweep_count++;
    }
}

static bool rotation_complete(void)
{
    return fsm_data.kobuki.rotation_complete != 0;
}

static void spin_rotate(void)
{
    Kobuki_Rotate_Angle(fsm_data.kobuki.angle);
}

static const FSM_Action spin_to_search = {
    .condition = rotation_complete,
    .next_state = FSM_ST_SEARCH,
};

// No condition: the do / of the State, runs when no Action above it ran.
static const FSM_Action spin_rotate_on = {
    .effect = spin_rotate,
};

static const FSM_Action *const spin_actions[] = {
    &action_fault_all,
    &spin_to_search,
    &spin_rotate_on,
};

const FSM_State state_spin = {
    .init = spin_init,
    .exit = spin_exit,
    .actions = spin_actions,
    .action_count = FSM_ARRAY_LEN(spin_actions),
};
