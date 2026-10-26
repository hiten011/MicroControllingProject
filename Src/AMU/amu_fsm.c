#include <stdint.h>
#include <stdbool.h>
#include "kobuki.h"
#include "ultrasonic.h"
#include "amu_config.h"
#include "amu_helpers.h"
#include "amu_fsm.h"

#define ORIENT_DEG ((int16_t)90)  // facing the blocking bollard -> facing the first corridor

static void Enter(AMU_Typedef *amu, AMU_State_Enum state)
{
    amu->state = state;
    amu->entry = true;
}

static bool Contact(const Kobuki_Typedef *kobuki)
{
    return (kobuki->bumper != 0U) || (kobuki->cliff != 0U);
}

// One loop of a one-shot rotation. The setpoint is issued on the entry loop only. The completion flag in the
// sensor snapshot still holds the previous move's value on that loop, so completion is tested from the next one.
// Returns true on the loop that sees the rotation complete, after stopping the motors.
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
        Enter(amu, AMU_PATHFIND);
    }
}

// Stub until the PATHFIND ticket.
static void Pathfind(AMU_Typedef *amu)
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
            Pathfind(amu);
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
