#include <stdint.h>
#include <stdbool.h>
#include "kobuki.h"
#include "ultrasonic.h"
#include "amu_config.h"
#include "amu_helpers.h"
#include "amu_fsm.h"

#define ORIENT_DEG        ((int16_t)90)   // facing the blocking bollard -> facing the first corridor
#define CORRIDOR_TURN_DEG ((int16_t)90)   // end of the first corridor -> facing the second corridor
#define STORAGE_TURN_DEG  ((int16_t)135)  // end of the second corridor -> facing the entrance

static void Enter(AMU_Typedef *amu, AMU_State_Enum state)
{
    amu->state = state;
    amu->entry = true;
}

static void Enter_Pathfind_State(AMU_Typedef *amu, AMU_Pathfind_Enum pathfind)
{
    amu->pathfind = pathfind;
    amu->entry = true;
}

// Entry to a corridor: latch the corridor heading and start in DRIVE.
static void Enter_Pathfind(AMU_Typedef *amu, const Kobuki_Typedef *kobuki, AMU_Corridor_Enum corridor)
{
    amu->corridor = corridor;
    amu->heading = kobuki->angle;
    Enter_Pathfind_State(amu, AMU_PF_DRIVE);
    Enter(amu, AMU_PATHFIND);
}

static void Enter_Storage(AMU_Typedef *amu)
{
    amu->storage = AMU_ST_TURN;
    Enter(amu, AMU_STORAGE);
}

// +1 in Layout A, -1 in Layout B: Layout B is the mirror image of Layout A.
// The layout is A or B here, ORIENT faults on none.
static int16_t Mirror(AMU_Layout_Enum layout)
{
    return (layout == AMU_LAYOUT_A) ? 1 : -1;
}

static bool Contact(const Kobuki_Typedef *kobuki)
{
    return (kobuki->bumper != 0U) || (kobuki->cliff != 0U);
}

// One loop of a one-shot rotation. The setpoint is issued on the entry loop only. The completion flag in the
// sensor snapshot still holds the previous move's value on that loop, so completion is tested from the next one.
// Returns true on the loop that sees the rotation complete, after stopping the motors.
// angle_deg is only used on the entry loop.
static bool Rotate_Step(AMU_Typedef *amu, const Kobuki_Typedef *kobuki, int16_t angle_deg)
{
    if (amu->entry) {
        amu->entry = false;
        Kobuki_Rotate_Angle_Setpoint(kobuki->angle, angle_deg);
    } else if (kobuki->rotation_complete) {
        Kobuki_Drive(STOP);
        return true;
    }
    Kobuki_Rotate_Angle(kobuki->angle);
    return false;
}

// The same for a drive of distance_mm.
static bool Drive_Step(AMU_Typedef *amu, const Kobuki_Typedef *kobuki, int16_t distance_mm)
{
    if (amu->entry) {
        amu->entry = false;
        Kobuki_Drive_Distance_Setpoint(kobuki->distance, distance_mm);
    } else if (kobuki->distance_complete) {
        Kobuki_Drive(STOP);
        return true;
    }
    Kobuki_Drive_Distance(kobuki->distance);
    return false;
}

static void Idle(AMU_Typedef *amu, const Kobuki_Typedef *kobuki)
{
    Kobuki_Drive(STOP);
    if ((kobuki->button & BUTTON_0_START) != 0U) {
        Enter(amu, AMU_SEARCH);
    }
}

static void Search(AMU_Typedef *amu, const Kobuki_Typedef *kobuki)
{
    US_Typedef object;

    if (amu->entry) {
        amu->entry = false;
        amu->layout = AMU_LAYOUT_NONE;
        amu->last_angle = kobuki->angle;
        amu->swept_deg = 0;
        amu->sweep_count = 0;
    }

    if (Contact(kobuki)) {
        Enter(amu, AMU_STOP_FAULT);
        return;
    }

    Ultrasonic_Detect(&object);
    if (object.detected) {
        // Stop first, then deduce the layout from this sample's range (the live range changes every loop).
        // The layout is re-deduced at every stop, so a SEARCH resumed after a rejected object overwrites it.
        Kobuki_Drive(STOP);
        amu->layout = AMU_Layout_From_Range(object.range_mm);
        Enter(amu, (amu->layout == AMU_LAYOUT_NONE) ? AMU_STOP_FAULT : AMU_ALIGN);
        return;
    }

    amu->swept_deg += AMU_Angle_Delta_Deg(amu->last_angle, kobuki->angle);
    amu->last_angle = kobuki->angle;
    if (amu->swept_deg >= 360) {
        amu->swept_deg -= 360;
        amu->sweep_count++;
    }
    if (amu->sweep_count >= MAX_SEARCH_SWEEPS) {
        Enter(amu, AMU_STOP_FAULT);
        return;
    }

    Kobuki_Rotate(SEARCH_SPEED);
}

// Stub until the ALIGN ticket: done at once, so the AMU still faces the blocking bollard it found.
static void Align(AMU_Typedef *amu, const Kobuki_Typedef *kobuki)
{
    if (kobuki->bumper != 0U) {
        Enter(amu, AMU_STOP_FAULT);
        return;
    }
    Enter(amu, AMU_ORIENT);
}

static void Orient(AMU_Typedef *amu, const Kobuki_Typedef *kobuki)
{
    int16_t angle_deg;

    if (Contact(kobuki)) {
        Enter(amu, AMU_STOP_FAULT);
        return;
    }

    // Saved layout only: the live range is not read again.
    switch (amu->layout) {
        case AMU_LAYOUT_A:
            angle_deg = -ORIENT_DEG;
            break;
        case AMU_LAYOUT_B:
            angle_deg = ORIENT_DEG;
            break;
        default:  // unreachable: SEARCH only leaves for ALIGN with a layout
            Enter(amu, AMU_STOP_FAULT);
            return;
    }

    if (Rotate_Step(amu, kobuki, angle_deg)) {
        Enter_Pathfind(amu, kobuki, AMU_FIRST_CORRIDOR);
    }
}

// End of a corridor. The first corridor turns towards the second. The second stops, and STORAGE turns from rest.
// There is no reverse (back-off) step.
static void Front_Cliff_Turn(AMU_Typedef *amu, const Kobuki_Typedef *kobuki)
{
    if (amu->corridor == AMU_SECOND_CORRIDOR) {
        Kobuki_Drive(STOP);
        Enter_Storage(amu);
    } else if (Rotate_Step(amu, kobuki, Mirror(amu->layout) * CORRIDOR_TURN_DEG)) {
        Enter_Pathfind(amu, kobuki, AMU_SECOND_CORRIDOR);
    }
}

// One shared sub-FSM for both corridors. Layout A has the storage wall on the left and the ledge on the right,
// Layout B is the mirror image. Bumper and cliff are triggers here, not faults.
static void Pathfind(AMU_Typedef *amu, const Kobuki_Typedef *kobuki)
{
    const int16_t mirror = Mirror(amu->layout);
    const uint8_t wall_bumper = (amu->layout == AMU_LAYOUT_A) ? BUMPER_LEFT : BUMPER_RIGHT;
    const uint8_t ledge_cliff = (amu->layout == AMU_LAYOUT_A) ? CLIFF_RIGHT : CLIFF_LEFT;

    switch (amu->pathfind) {
        case AMU_PF_DRIVE:
            amu->entry = false;
            // Priority: front cliff, then the storage wall bumper, then the ledge cliff. Wrong-side and centre
            // triggers are ignored. A trigger only changes sub-state, so no drive command shares its loop with
            // the turn that follows.
            if ((kobuki->cliff & CLIFF_CENTRE) != 0U) {
                Enter_Pathfind_State(amu, AMU_PF_FRONT_CLIFF_TURN);
            } else if ((kobuki->bumper & wall_bumper) != 0U) {
                Enter_Pathfind_State(amu, AMU_PF_WALL_HIT);
            } else if ((kobuki->cliff & ledge_cliff) != 0U) {
                Enter_Pathfind_State(amu, AMU_PF_LEDGE_HIT);
            } else {
                Kobuki_Drive(PATH_SPEED);
            }
            break;
        case AMU_PF_WALL_HIT:
            if (Rotate_Step(amu, kobuki, -mirror * WALL_DEG)) {
                Enter_Pathfind_State(amu, AMU_PF_REALIGN);
            }
            break;
        case AMU_PF_LEDGE_HIT:
            if (Rotate_Step(amu, kobuki, mirror * LEDGE_DEG)) {
                Enter_Pathfind_State(amu, AMU_PF_REALIGN);
            }
            break;
        case AMU_PF_REALIGN:
            if (Rotate_Step(amu, kobuki, AMU_Angle_Delta_Deg(kobuki->angle, amu->heading))) {
                Enter_Pathfind_State(amu, AMU_PF_DRIVE);
            }
            break;
        case AMU_PF_FRONT_CLIFF_TURN:
            Front_Cliff_Turn(amu, kobuki);
            break;
    }
}

// Bumper and cliff are not faults here. The turn is made beside the ledge, so both are ignored. A bumper hit
// during the drive-in ends it.
static void Storage(AMU_Typedef *amu, const Kobuki_Typedef *kobuki)
{
    switch (amu->storage) {
        case AMU_ST_TURN:
            if (Rotate_Step(amu, kobuki, Mirror(amu->layout) * STORAGE_TURN_DEG)) {
                amu->storage = AMU_ST_DRIVE_IN;
                amu->entry = true;
            }
            break;
        case AMU_ST_DRIVE_IN:
            if (kobuki->bumper != 0U) {
                Kobuki_Drive(STOP);
                Enter(amu, AMU_DEPOSIT);
            } else if (Drive_Step(amu, kobuki, ENTRY_MM)) {
                Enter(amu, AMU_DEPOSIT);
            }
            break;
    }
}

// Stub until the DEPOSIT ticket.
static void Deposit(AMU_Typedef *amu)
{
    Kobuki_Drive(STOP);
    Enter(amu, AMU_IDLE);
}

static void Stop_Fault(AMU_Typedef *amu)
{
    Kobuki_Drive(STOP);
    Enter(amu, AMU_IDLE);
}

static void Run_State(AMU_Typedef *amu, const Kobuki_Typedef *kobuki)
{
    switch (amu->state) {
        case AMU_IDLE:
            Idle(amu, kobuki);
            break;
        case AMU_SEARCH:
            Search(amu, kobuki);
            break;
        case AMU_ALIGN:
            Align(amu, kobuki);
            break;
        case AMU_ORIENT:
            Orient(amu, kobuki);
            break;
        case AMU_PATHFIND:
            Pathfind(amu, kobuki);
            break;
        case AMU_STORAGE:
            Storage(amu, kobuki);
            break;
        case AMU_DEPOSIT:
            Deposit(amu);
            break;
        case AMU_STOP_FAULT:
            Stop_Fault(amu);
            break;
    }
}

void AMU_Init(AMU_Typedef *amu)
{
    amu->layout = AMU_LAYOUT_NONE;
    amu->last_angle = 0;
    amu->swept_deg = 0;
    amu->sweep_count = 0;
    amu->corridor = AMU_FIRST_CORRIDOR;
    amu->pathfind = AMU_PF_DRIVE;
    amu->storage = AMU_ST_TURN;
    amu->heading = 0;
    Enter(amu, AMU_IDLE);
}

void AMU_Step(AMU_Typedef *amu, const Kobuki_Typedef *kobuki)
{
    // B1 or a wheel drop stops the run from any state but IDLE.
    if (amu->state != AMU_IDLE && ((kobuki->button & BUTTON_1_STOP) != 0U || kobuki->wheeldrop != 0U)) {
        Enter(amu, AMU_STOP_FAULT);
    }

    Run_State(amu, kobuki);

    // A fault raised in this loop stops the motors in the same loop.
    if (amu->state == AMU_STOP_FAULT) {
        Run_State(amu, kobuki);
    }
}
