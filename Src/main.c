#include "my_stm_helper.h"
#include "uart.h"
#include "adc.h"
#include "helper.h"

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
    ST_DEPOSIT_ALIGN,
    ST_DEPOSIT_DISTANCE,
    ST_DEPOSIT_REVERSE,
    ST_DEPOSIT_APPROACH,
    ST_FAULT
} State;

static State state = ST_IDLE;

// A transition runs the old State's exit, then the new State's entry.
static void change_to(State next)
{
    switch (state) {
        case ST_IDLE:
            robot.layout = LAYOUT_NONE;
            robot.object.detected = 0;
            robot.heading = 0;
            robot.corridor = 0;
            Scan_Start(robot.kobuki.angle);
            break;

        case ST_CORRIDOR_TURN:
            robot.heading = robot.kobuki.angle;
            break;

        default:
            break;
    }

    state = next;

    switch (next) {
        case ST_IDENTIFY:
        case ST_CORRIDOR_STOP:
        case ST_FAULT:
            Kobuki_Drive(STOP);
            break;

        case ST_DEPOSIT:
            Kobuki_Drive(STOP);
            Deposit_Scan_Start(robot.kobuki.angle);
            Kobuki_Update_LEDs(OFF, OFF);
            break;

        case ST_DEPOSIT_DISTANCE:
            Kobuki_Drive(STOP);
            robot.deposit_settle_packets = DEPOSIT_SETTLE_PACKETS;
            Kobuki_Update_LEDs(YELLOW, OFF);
            break;

        case ST_ORIENT:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, Layout_Pick(ORIENT_A_DEG, ORIENT_B_DEG));
            break;

        case ST_REVERSE:
        case ST_CORRIDOR_REVERSE:
            Kobuki_Drive_Distance_Setpoint(robot.kobuki.distance, -PATHFIND_REVERSE_MM);
            break;

        case ST_ROTATE:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, robot.pathfind_recover_deg);
            break;

        case ST_CORRIDOR_TURN:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, Layout_Pick(PATHFIND_CORNER_A_DEG, PATHFIND_CORNER_B_DEG));
            break;

        case ST_TURN:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, Layout_Pick(STORAGE_TURN_A_DEG, STORAGE_TURN_B_DEG));
            break;

        case ST_DRIVE_IN:
            Kobuki_Drive_Distance_Setpoint(robot.kobuki.distance, STORAGE_DRIVE_IN_MM);
            break;

        default:
            break;
    }
}

int main(void)
{
    UART_Init();
    ADC_Init();
    Ultrasonic_Init();

    if (DEPOSIT_TEST_MODE) {
        // Use actual feedback for the initial gyro heading before starting the test scan.
        Kobuki_Read(&robot.kobuki);
        change_to(ST_DEPOSIT);
    }

    while (1) {
        Kobuki_Read(&robot.kobuki);

        switch (state) {
            case ST_IDLE:
                if ((robot.kobuki.button & BUTTON_B0) != 0) {
                    change_to(DEPOSIT_TEST_MODE ? ST_DEPOSIT : ST_SEARCH);
                    break;
                }
                Kobuki_Drive(STOP);
                break;

            // SEARCH phase
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
                    Kobuki_Update_LEDs(GREEN, NONE);
                    change_to(ST_FAULT);
                    break;
                }
                Kobuki_Rotate(SCAN_SPEED_MM_S);
                break;

            case ST_IDENTIFY:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                robot.layout = Layout_From_Range(robot.object.range_mm);
                Kobuki_Update_LEDs((robot.layout == LAYOUT_A) ? RED : YELLOW, NONE);
                change_to(ST_ORIENT);
                break;

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

            // PATHFIND phase
            case ST_DRIVE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (Kobuki_IsCliffDetected(&robot.kobuki, SIDE_CENTRE)) {
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
                Kobuki_Drive(PATHFIND_DRIVE_SPEED);
                break;

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

            case ST_CORRIDOR_STOP:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                change_to(ST_TURN);
                break;

            // STORAGE phase
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

            case ST_DEPOSIT:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                Deposit_Scan_Sample();
                if (Scan_Gave_Up()) {
                    Kobuki_Drive(STOP);
                    if (robot.deposit_rod.detected) {
                        change_to(ST_DEPOSIT_ALIGN);
                    } else {
                        Kobuki_Update_LEDs(RED, OFF);
                        change_to(ST_IDLE);
                    }
                    break;
                }
                Kobuki_Rotate(DEPOSIT_SCAN_SPEED_MM_S);
                break;

            case ST_DEPOSIT_ALIGN: {
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                int16_t turn = Deposit_Rod_Turn_Ticks();
                if (turn >= -DEPOSIT_ALIGN_TOLERANCE_TICKS && turn <= DEPOSIT_ALIGN_TOLERANCE_TICKS) {
                    change_to(ST_DEPOSIT_DISTANCE);
                    break;
                }
                Kobuki_Rotate(turn > 0 ? DEPOSIT_SCAN_SPEED_MM_S : -DEPOSIT_SCAN_SPEED_MM_S);
                break;
            }

            case ST_DEPOSIT_DISTANCE:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.deposit_settle_packets > 0) {
                    robot.deposit_settle_packets--;
                    Kobuki_Drive(STOP);
                    break;
                }
                if (!Deposit_Read_Range()) {
                    Kobuki_Update_LEDs(RED, OFF);
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.deposit_range_mm < DEPOSIT_TARGET_RANGE_MM - DEPOSIT_RANGE_TOLERANCE_MM) {
                    change_to(ST_DEPOSIT_REVERSE);
                } else if (robot.deposit_range_mm > DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM) {
                    change_to(ST_DEPOSIT_APPROACH);
                } else {
                    Kobuki_Update_LEDs(GREEN, OFF);
                    change_to(ST_IDLE);
                }
                break;

            case ST_DEPOSIT_REVERSE:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (!Deposit_Read_Range()) {
                    Kobuki_Update_LEDs(RED, OFF);
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.deposit_range_mm >= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_REVERSE_MARGIN_MM) {
                    change_to(ST_DEPOSIT_DISTANCE);
                    break;
                }
                Kobuki_Drive(-DEPOSIT_REVERSE_SPEED);
                break;

            case ST_DEPOSIT_APPROACH:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (!Deposit_Read_Range()) {
                    Kobuki_Update_LEDs(RED, OFF);
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.deposit_range_mm <= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM) {
                    change_to(ST_DEPOSIT_DISTANCE);
                    break;
                }
                Kobuki_Drive(DEPOSIT_APPROACH_SPEED);
                break;

            case ST_FAULT:
                change_to(ST_IDLE);
                break;
        }
    }
}
