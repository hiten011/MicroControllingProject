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

    // DEPOSIT phase
    ST_DEPOSIT_SEARCH,
    ST_DEPOSIT_TRACK,
    ST_DEPOSIT_EDGE_SCAN,
    ST_DEPOSIT_ALIGN,
    ST_MEASURE,
    ST_DEPOSIT_BACKOFF,
    ST_DEPOSIT_DRIVE,
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

        case ST_DEPOSIT_SEARCH:
            Scan_Start(robot.kobuki.angle);
            robot.backoffs = 0;
            break;

        case ST_DEPOSIT_EDGE_SCAN:
            Deposit_Edge_Start();
            break;

        case ST_DEPOSIT_ALIGN:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, (int16_t)(robot.centre_deg - robot.scan_deg));
            break;

        case ST_MEASURE:
            Kobuki_Drive(STOP);
            robot.samples = 0;
            break;

        case ST_DEPOSIT_BACKOFF:
            robot.backoffs++;
            Kobuki_Drive_Distance_Setpoint(robot.kobuki.distance, -DEPOSIT_BACKOFF_MM);
            break;

        case ST_DEPOSIT_DRIVE:
            Kobuki_Drive_Distance_Setpoint(robot.kobuki.distance, robot.drive_mm);
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

    while (1) {
        Kobuki_Read(&robot.kobuki);

        switch (state) {
            case ST_IDLE:
                if ((robot.kobuki.button & BUTTON_B0) != 0) {
                    change_to(ST_SEARCH);
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
                    change_to(ST_DEPOSIT_SEARCH);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(ST_DEPOSIT_SEARCH);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            // DEPOSIT phase
            case ST_DEPOSIT_SEARCH:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                Scan_Sample();
                if (Deposit_Marker_Seen()) {
                    robot.last_seen = robot.scan_deg;
                    robot.closest = robot.object.range_mm;
                    change_to(ST_DEPOSIT_TRACK);
                    break;
                }
                Kobuki_Rotate(DEPOSIT_SCAN_SPEED_MM_S);
                break;

            case ST_DEPOSIT_TRACK:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                Scan_Sample();
                Deposit_Track_Sample();
                if (Deposit_Marker_Lost()) {
                    change_to(ST_DEPOSIT_EDGE_SCAN);
                    break;
                }
                Kobuki_Rotate(DEPOSIT_SCAN_SPEED_MM_S);
                break;

            case ST_DEPOSIT_EDGE_SCAN:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                Scan_Sample();
                Deposit_Edge_Sample();
                if (Deposit_Edge_Done()) {
                    robot.centre_deg = (int16_t)((robot.edge_start + robot.edge_end) / 2);
                    change_to(ST_DEPOSIT_ALIGN);
                    break;
                }
                Kobuki_Rotate(-DEPOSIT_SCAN_SPEED_MM_S);
                break;

            case ST_DEPOSIT_ALIGN:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(ST_MEASURE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            case ST_MEASURE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (!Measure_Take_Reading()) {
                    break;
                }
                if (robot.reading < DEPOSIT_BLIND_MM) {
                    change_to((robot.backoffs < DEPOSIT_BACKOFF_MAX) ? ST_DEPOSIT_BACKOFF : ST_IDLE);
                    break;
                }
                change_to((robot.drive_mm == 0) ? ST_IDLE : ST_DEPOSIT_DRIVE);
                break;

            case ST_DEPOSIT_BACKOFF:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(ST_MEASURE);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            case ST_DEPOSIT_DRIVE:
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.drive_mm > 0 && robot.kobuki.bumper != 0) {
                    Kobuki_Drive(STOP);
                    change_to(ST_IDLE);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(ST_IDLE);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            case ST_FAULT:
                change_to(ST_IDLE);
                break;
        }
    }
}
