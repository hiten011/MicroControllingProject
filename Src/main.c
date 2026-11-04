#include "uart.h"
#include "adc.h"
#include "helper.h"

// The AMU's finite-state-machine states, grouped by phase.
typedef enum {
    STATE_IDLE,

    // SEARCH phase
    STATE_SEARCH,
    STATE_IDENTIFY,

    STATE_ORIENT,

    // PATHFIND phase
    STATE_DRIVE,
    STATE_REVERSE,
    STATE_ROTATE,
    STATE_CORRIDOR_REVERSE,
    STATE_CORRIDOR_TURN,
    STATE_CORRIDOR_STOP,

    // STORAGE phase
    STATE_TURN,
    STATE_DRIVE_IN,
    STATE_DEPOSIT,
    STATE_DEPOSIT_ALIGN,
    STATE_DEPOSIT_REFINE_TURN,
    STATE_DEPOSIT_REFINE_SAMPLE,
    STATE_DEPOSIT_DISTANCE,
    STATE_DEPOSIT_REVERSE,
    STATE_DEPOSIT_APPROACH,
    STATE_FAULT
} State;

// Current state, only changed through change_to().
static State state = STATE_IDLE;

// A transition runs the old State's exit, then the new State's entry.
static void change_to(State next)
{
    switch (state) {
        case STATE_IDLE:
            // Leaving IDLE starts a new run, so clear the previous one and cut any win sound still playing.
            Win_Sound_Stop();
            robot.layout = LAYOUT_NONE;
            robot.object.detected = 0;
            robot.heading = 0;
            robot.corridor = 0;
            Scan_Start(robot.kobuki.angle);
            break;

        case STATE_CORRIDOR_TURN:
            // Remember the heading the corner turn ended on.
            robot.heading = robot.kobuki.angle;
            break;

        default:
            break;
    }

    state = next;

    switch (next) {
        case STATE_IDENTIFY:
        case STATE_CORRIDOR_STOP:
        case STATE_FAULT:
            Kobuki_Drive(STOP);
            break;

        case STATE_DEPOSIT:
            Kobuki_Drive(STOP);
            Deposit_Scan_Start(robot.kobuki.angle);
            Kobuki_Update_LEDs(OFF, OFF);
            break;

        case STATE_DEPOSIT_DISTANCE:
            Kobuki_Drive(STOP);
            robot.deposit_settle_packets = DEPOSIT_SETTLE_PACKETS;
            Kobuki_Update_LEDs(YELLOW, OFF);
            break;

        case STATE_DEPOSIT_REFINE_SAMPLE:
            Kobuki_Drive(STOP);
            Deposit_Refine_Settle();
            break;

        case STATE_ORIENT:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle,
                                         Layout_Pick(ORIENT_TURN_LAYOUT_A_DEG, ORIENT_TURN_LAYOUT_B_DEG));
            break;

        case STATE_REVERSE:
        case STATE_CORRIDOR_REVERSE:
            Kobuki_Drive_Distance_Setpoint(robot.kobuki.distance, -PATHFIND_REVERSE_MM);
            break;

        case STATE_ROTATE:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, robot.pathfind_recover_deg);
            break;

        case STATE_CORRIDOR_TURN:
            Kobuki_Rotate_Angle_Setpoint(
                robot.kobuki.angle, Layout_Pick(PATHFIND_CORNER_TURN_LAYOUT_A_DEG, PATHFIND_CORNER_TURN_LAYOUT_B_DEG));
            break;

        case STATE_TURN:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle,
                                         Layout_Pick(STORAGE_TURN_LAYOUT_A_DEG, STORAGE_TURN_LAYOUT_B_DEG));
            break;

        case STATE_DRIVE_IN:
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
            // Wait for B0, staying stopped, and play out the win sound after a deposit.
            case STATE_IDLE:
                if ((robot.kobuki.button & KOBUKI_BUTTON_B0_MASK) != 0) {
                    change_to(STATE_SEARCH);
                    break;
                }
                Kobuki_Drive(STOP);
                Win_Sound_Tick();
                break;

            // SEARCH phase: spin on the spot until the ultrasonic sees a bollard, or give up after a full turn.
            case STATE_SEARCH:
                if (Fault_Any()) {
                    change_to(STATE_FAULT);
                    break;
                }
                Scan_Sample();
                if (robot.object.detected != 0) {
                    change_to(STATE_IDENTIFY);
                    break;
                }
                if (Scan_Gave_Up()) {
                    Kobuki_Update_LEDs(GREEN, OFF);
                    change_to(STATE_FAULT);
                    break;
                }
                Kobuki_Rotate(SCAN_ROTATE_SPEED_MM_S);
                break;

            // Pick the Layout from the first range reading and show it on the LED.
            case STATE_IDENTIFY:
                if (Fault_Any()) {
                    change_to(STATE_FAULT);
                    break;
                }
                robot.layout = Layout_From_Range(robot.object.range_mm);
                Kobuki_Update_LEDs((robot.layout == LAYOUT_A) ? RED : YELLOW, OFF);
                change_to(STATE_ORIENT);
                break;

            // Turn towards the first corridor.
            case STATE_ORIENT:
                if (Fault_Any()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    robot.heading = robot.kobuki.angle;
                    robot.corridor = 1;
                    change_to(STATE_DRIVE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            // PATHFIND phase: drive the corridor, backing off and nudging away from walls and ledges.
            case STATE_DRIVE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (Kobuki_IsCliffDetected(&robot.kobuki, KOBUKI_SIDE_CENTRE_MASK)) {
                    change_to((robot.corridor == 1) ? STATE_CORRIDOR_REVERSE : STATE_CORRIDOR_STOP);
                    break;
                }
                if (Corridor_Take_Wall_Hit(robot.kobuki.bumper)) {
                    change_to(STATE_REVERSE);
                    break;
                }
                if (Corridor_Take_Ledge_Hit(&robot.kobuki)) {
                    change_to(STATE_REVERSE);
                    break;
                }
                Kobuki_Drive(PATHFIND_DRIVE_SPEED_MM_S);
                break;

            // Back off a wall or ledge hit.
            case STATE_REVERSE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(STATE_ROTATE);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            // Nudge away from the wall or ledge, then resume driving.
            case STATE_ROTATE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(STATE_DRIVE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            // Turn the corner into the second corridor.
            case STATE_CORRIDOR_TURN:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    robot.corridor = 2;
                    change_to(STATE_DRIVE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            // Back off the front cliff before the corner turn.
            case STATE_CORRIDOR_REVERSE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(STATE_CORRIDOR_TURN);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            // Reached the end of the second corridor; stop, then go to the storage phase.
            case STATE_CORRIDOR_STOP:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                change_to(STATE_TURN);
                break;

            // STORAGE phase: face the storage unit, drive in, then find and settle at the deposit target.
            case STATE_TURN:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(STATE_DRIVE_IN);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            // Drive in until the distance is done or a bumper touches the unit.
            case STATE_DRIVE_IN:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.kobuki.bumper != 0) {
                    Kobuki_Drive(STOP);
                    change_to(STATE_DEPOSIT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(STATE_DEPOSIT);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            // Turn a full circle anticlockwise, remembering the closest valid ultrasonic target and its heading.
            case STATE_DEPOSIT:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                Deposit_Scan_Sample();
                if (Scan_Gave_Up()) {
                    Kobuki_Drive(STOP);
                    if (robot.deposit_rod.detected) {
                        change_to(STATE_DEPOSIT_ALIGN);
                    } else {
                        Kobuki_Update_LEDs(RED, OFF);
                        change_to(STATE_IDLE);
                    }
                    break;
                }
                Kobuki_Rotate(DEPOSIT_SCAN_SPEED_MM_S);
                break;

            // Turn the short way round to face the saved target heading: first the coarse one, which starts the
            // refine sweep, then the refined one, which goes on to the distance check.
            case STATE_DEPOSIT_ALIGN: {
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                int16_t turn = Deposit_Rod_Turn_Ticks();
                int16_t tolerance =
                    robot.deposit_refined ? DEPOSIT_REFINE_TOLERANCE_TICKS : DEPOSIT_ALIGN_TOLERANCE_TICKS;
                if (turn >= -tolerance && turn <= tolerance) {
                    Kobuki_Drive(STOP);
                    if (robot.deposit_refined) {
                        change_to(STATE_DEPOSIT_DISTANCE);
                    } else {
                        Deposit_Refine_Start();
                        change_to(STATE_DEPOSIT_REFINE_TURN);
                    }
                    break;
                }
                int16_t speed = robot.deposit_refined ? DEPOSIT_REFINE_SPEED_MM_S : DEPOSIT_SCAN_SPEED_MM_S;
                Kobuki_Rotate(turn > 0 ? speed : -speed);
                break;
            }

            // Turn to the next step of the refine sweep around the coarse heading.
            case STATE_DEPOSIT_REFINE_TURN: {
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                int16_t turn = Deposit_Refine_Turn_Ticks();
                if (turn >= -DEPOSIT_REFINE_TOLERANCE_TICKS && turn <= DEPOSIT_REFINE_TOLERANCE_TICKS) {
                    change_to(STATE_DEPOSIT_REFINE_SAMPLE);
                    break;
                }
                Kobuki_Rotate(turn > 0 ? DEPOSIT_REFINE_SPEED_MM_S : -DEPOSIT_REFINE_SPEED_MM_S);
                break;
            }

            // Stopped at a sweep step: take three readings, then move to the next step or pick the refined heading.
            case STATE_DEPOSIT_REFINE_SAMPLE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(STATE_FAULT);
                    break;
                }
                Kobuki_Drive(STOP);
                if (!Deposit_Refine_Sample()) {
                    break;
                }
                if (Deposit_Refine_Next()) {
                    change_to(STATE_DEPOSIT_REFINE_TURN);
                } else if (Deposit_Refine_Finish()) {
                    change_to(STATE_DEPOSIT_ALIGN);
                } else {
                    Kobuki_Update_LEDs(RED, OFF);
                    change_to(STATE_FAULT);
                }
                break;

            // Stopped: wait for the ultrasonic to catch up, then check the live range and pick the next move.
            case STATE_DEPOSIT_DISTANCE:
                if (Fault_Any()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.deposit_settle_packets > 0) {
                    robot.deposit_settle_packets--;
                    Kobuki_Drive(STOP);
                    break;
                }
                if (!Deposit_Read_Range()) {
                    Kobuki_Update_LEDs(RED, OFF);
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.deposit_range_mm < DEPOSIT_TARGET_RANGE_MM - DEPOSIT_RANGE_TOLERANCE_MM) {
                    change_to(STATE_DEPOSIT_REVERSE);
                } else if (robot.deposit_range_mm > DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM) {
                    change_to(STATE_DEPOSIT_APPROACH);
                } else {
                    Kobuki_Update_LEDs(GREEN, OFF);
                    Win_Sound_Start();
                    change_to(STATE_IDLE);
                }
                break;

            // Too close: back off past the target so the final correction is made moving forward.
            case STATE_DEPOSIT_REVERSE:
                if (Fault_Any()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (!Deposit_Read_Range()) {
                    Kobuki_Update_LEDs(RED, OFF);
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.deposit_range_mm >= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_REVERSE_MARGIN_MM) {
                    change_to(STATE_DEPOSIT_DISTANCE);
                    break;
                }
                Kobuki_Drive(-DEPOSIT_REVERSE_SPEED_MM_S);
                break;

            // Too far: creep forward until the reading is back inside the tolerance band.
            case STATE_DEPOSIT_APPROACH:
                if (Fault_Any()) {
                    change_to(STATE_FAULT);
                    break;
                }
                if (!Deposit_Read_Range()) {
                    Kobuki_Update_LEDs(RED, OFF);
                    change_to(STATE_FAULT);
                    break;
                }
                if (robot.deposit_range_mm <= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM) {
                    change_to(STATE_DEPOSIT_DISTANCE);
                    break;
                }
                Kobuki_Drive(DEPOSIT_APPROACH_SPEED_MM_S);
                break;

            // Fault: motors already stopped on entry, so just go back to IDLE.
            case STATE_FAULT:
                change_to(STATE_IDLE);
                break;
        }
    }
}
