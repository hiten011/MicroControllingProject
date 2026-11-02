#include "uart.h"
#include "adc.h"
#include "helper.h"

// The AMU's finite-state-machine states, grouped by phase.
typedef enum {
    ST_IDLE,

    // SEARCH phase
    ST_SEARCH,
    ST_IDENTIFY,

    ST_ORIENT,

    // PATHFIND phase
    ST_DRIVE,
    ST_REVERSE,
    ST_ROTATE,
    ST_CORRIDOR_REVERSE,
    ST_CORRIDOR_TURN,
    ST_CORRIDOR_STOP,

    // STORAGE phase
    ST_TURN,
    ST_DRIVE_IN,
    ST_DEPOSIT,
    ST_FAULT
} State;

// Current state, only changed through change_to().
static State state = ST_IDLE;

// A transition runs the old State's exit, then the new State's entry.
static void change_to(State next)
{
    switch (state) {
        case ST_IDLE:
            // Leaving IDLE starts a new run, so clear the previous one.
            robot.layout = LAYOUT_NONE;
            robot.object.detected = 0;
            robot.heading = 0;
            robot.corridor = 0;
            Scan_Start(robot.kobuki.angle);
            break;

        case ST_CORRIDOR_TURN:
            // Remember the heading the corner turn ended on.
            robot.heading = robot.kobuki.angle;
            break;

        default:
            break;
    }

    state = next;

    switch (next) {
        case ST_IDENTIFY:
        case ST_CORRIDOR_STOP:
        case ST_DEPOSIT:
        case ST_FAULT:
            Kobuki_Drive(STOP);
            break;

        case ST_ORIENT:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle,
                                         Layout_Pick(ORIENT_TURN_LAYOUT_A_DEG, ORIENT_TURN_LAYOUT_B_DEG));
            break;

        case ST_REVERSE:
        case ST_CORRIDOR_REVERSE:
            Kobuki_Drive_Distance_Setpoint(robot.kobuki.distance, -PATHFIND_REVERSE_MM);
            break;

        case ST_ROTATE:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, robot.pathfind_recover_deg);
            break;

        case ST_CORRIDOR_TURN:
            Kobuki_Rotate_Angle_Setpoint(
                robot.kobuki.angle, Layout_Pick(PATHFIND_CORNER_TURN_LAYOUT_A_DEG, PATHFIND_CORNER_TURN_LAYOUT_B_DEG));
            break;

        case ST_TURN:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle,
                                         Layout_Pick(STORAGE_TURN_LAYOUT_A_DEG, STORAGE_TURN_LAYOUT_B_DEG));
            break;

        case ST_DRIVE_IN:
            Kobuki_Drive_Distance_Setpoint(robot.kobuki.distance, STORAGE_DRIVE_IN_MM);
            break;

        default:
            break;
    }
}

// Initialise the peripherals, then run one state step per received Kobuki packet.
int main(void)
{
    UART_Init();
    ADC_Init();
    Ultrasonic_Init();

    while (1) {
        // Blocks until the next packet, which paces the loop.
        Kobuki_Read(&robot.kobuki);

        switch (state) {
            // Wait for B0, staying stopped.
            case ST_IDLE:
                if ((robot.kobuki.button & KOBUKI_BUTTON_B0_MASK) != 0) {
                    change_to(ST_SEARCH);
                    break;
                }
                Kobuki_Drive(STOP);
                break;

            // SEARCH phase: spin on the spot until the ultrasonic sees a bollard, or give up after a full turn.
            case ST_SEARCH:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                Scan_Sample();
                if (robot.object.detected != 0) {
                    change_to(ST_IDENTIFY);
                    break;
                }
                if (Scan_Gave_Up()) {
                    Kobuki_Update_LEDs(GREEN, OFF);
                    change_to(ST_FAULT);
                    break;
                }
                Kobuki_Rotate(SCAN_ROTATE_SPEED_MM_S);
                break;

            // Pick the Layout from the first range reading and show it on the LED.
            case ST_IDENTIFY:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                robot.layout = Layout_From_Range(robot.object.range_mm);
                Kobuki_Update_LEDs((robot.layout == LAYOUT_A) ? RED : YELLOW, OFF);
                change_to(ST_ORIENT);
                break;

            // Turn towards the first corridor.
            case ST_ORIENT:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    robot.heading = robot.kobuki.angle;
                    robot.corridor = 1;
                    change_to(ST_DRIVE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            // PATHFIND phase: drive the corridor, backing off and nudging away from walls and ledges.
            case ST_DRIVE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (Kobuki_IsCliffDetected(&robot.kobuki, KOBUKI_SIDE_CENTRE_MASK)) {
                    change_to((robot.corridor == 1) ? ST_CORRIDOR_REVERSE : ST_CORRIDOR_STOP);
                    break;
                }
                if (Corridor_Take_Wall_Hit(robot.kobuki.bumper)) {
                    change_to(ST_REVERSE);
                    break;
                }
                if (Corridor_Take_Ledge_Hit(&robot.kobuki)) {
                    change_to(ST_REVERSE);
                    break;
                }
                Kobuki_Drive(PATHFIND_DRIVE_SPEED_MM_S);
                break;

            // Back off a wall or ledge hit.
            case ST_REVERSE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(ST_ROTATE);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            // Nudge away from the wall or ledge, then resume driving.
            case ST_ROTATE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(ST_DRIVE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            // Turn the corner into the second corridor.
            case ST_CORRIDOR_TURN:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    robot.corridor = 2;
                    change_to(ST_DRIVE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            // Back off the front cliff before the corner turn.
            case ST_CORRIDOR_REVERSE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(ST_CORRIDOR_TURN);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            // Reached the end of the second corridor; stop, then go to the storage phase.
            case ST_CORRIDOR_STOP:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                change_to(ST_TURN);
                break;

            // STORAGE phase: face the storage unit, drive in, and stop.
            case ST_TURN:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(ST_DRIVE_IN);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            // Drive in until the distance is done or a bumper touches the unit.
            case ST_DRIVE_IN:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.bumper != 0) {
                    Kobuki_Drive(STOP);
                    change_to(ST_DEPOSIT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(ST_DEPOSIT);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            // Parcel delivered; return to IDLE for the next run.
            case ST_DEPOSIT:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                change_to(ST_IDLE);
                break;

            // Fault: motors already stopped on entry, so just go back to IDLE.
            case ST_FAULT:
                change_to(ST_IDLE);
                break;
        }
    }
}
