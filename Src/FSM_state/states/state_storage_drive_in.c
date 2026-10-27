#include "action_fault.h"
#include "fsm_states.h"

// Drive a fixed distance through the Entrance. Cliff ignored; any bumper aborts to Deposit.
static void storage_drive_in_init(void)
{
    Kobuki_Drive_Distance_Setpoint(fsm_data.kobuki.distance, FSM_STORAGE_DRIVE_IN_MM);
}

static bool any_bumper(void)
{
    return fsm_data.kobuki.bumper != 0;
}

static bool distance_complete(void)
{
    return fsm_data.kobuki.distance_complete != 0;
}

static void stop_drive(void)
{
    Kobuki_Drive(STOP);
}

static void storage_drive_in_drive(void)
{
    Kobuki_Drive_Distance(fsm_data.kobuki.distance);
}

static const FSM_Action storage_drive_in_bumper = {
    .condition = any_bumper,
    .effect = stop_drive,
    .next_state = FSM_ST_STORAGE_DEPOSIT,
};

static const FSM_Action storage_drive_in_to_deposit = {
    .condition = distance_complete,
    .next_state = FSM_ST_STORAGE_DEPOSIT,
};

static const FSM_Action storage_drive_in_drive_on = {
    .effect = storage_drive_in_drive,
};

static const FSM_Action *const storage_drive_in_actions[] = {
    &action_fault_b1_wheeldrop,
    &storage_drive_in_bumper,
    &storage_drive_in_to_deposit,
    &storage_drive_in_drive_on,
};

const FSM_State state_storage_drive_in = {
    .init = storage_drive_in_init,
    .actions = storage_drive_in_actions,
    .action_count = FSM_ARRAY_LEN(storage_drive_in_actions),
};
