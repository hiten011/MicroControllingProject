#include "uart.h"
#include "adc.h"
#include "helper.h"
#include <stdint.h>

#define DEPOSIT_SCAN_SPEED_MM_S 60  // Shaheer: Turning speed. can increase or decrease 
#define DEPOSIT_SCAN_PASSES 2 // Shaheer: Can change to 3 this is mostly to double check
#define DEPOSIT_BEARING_TOL_DEG 10 // Shaheer: angle tolerace between the two scan
#define DEPOSIT_MAX_RANGE_MM 700
#define DEPOSIT_MIN_RANGE_MM 70
#define DEPOSIT_TARGET_MM 300 // Shaheer: Mess around with Tolerances
#define DEPOSIT_TOL_MM 25
#define DEPOSIT_STEP_MM 60 // Shaheer: Small adjustment steps
#define DEPOSIT_MAX_ADJUSTMENTS 12
#define DEPOSIT_LED_BLINK_LOOPS 5000U // LED 2 blinking configuration

// Deposit variables
static uint8_t deposit_passes = 0;
static uint8_t deposit_hits = 0;
static uint8_t deposit_adjustments = 0;
static uint16_t deposit_best_range = UINT16_MAX;
static int16_t deposit_bearing_tick = 0;
static int16_t deposit_candidate_tick = 0;
static int16_t deposit_move_mm = 0;
static uint32_t deposit_led_blink_count = 0;
static uint8_t deposit_led_on = 0;

// Calculate the difference between two angles
static int16_t Deposit_Angle_Difference(int16_t target, int16_t current)
{
    int32_t diff = (int32_t)target - current;
    while (diff > 18000)
        diff -= 36000;
    while (diff < -18000)
        diff += 36000;
    return (int16_t)diff;
}

// Reset ultrasonic scanning
static void Deposit_Reset_Pass(void)
{
    deposit_best_range = UINT16_MAX;
    Scan_Start(robot.kobuki.angle);
}

// Robot states
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
    ST_DEPOSIT,
    ST_DEPOSIT_SCAN,
    ST_DEPOSIT_ALIGN,
    ST_DEPOSIT_ADJUST,
    ST_DEPOSIT_MOVE,
    ST_DEPOSIT_DONE,
    ST_FAULT
} State;

static State state = ST_IDLE;

// Change robot state
static void change_to(State next)
{
    // Exit actions
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
    // Entry actions
    switch (next) {
        case ST_IDENTIFY:
        case ST_CORRIDOR_STOP:
        case ST_DEPOSIT:
        case ST_DEPOSIT_SCAN:
        case ST_DEPOSIT_ADJUST:
        case ST_DEPOSIT_DONE:
        case ST_FAULT:
            Kobuki_Drive(STOP);
            if (next == ST_DEPOSIT) {
                deposit_passes = 0;
                deposit_hits = 0;
                deposit_adjustments = 0;
                deposit_candidate_tick = 0;
                Deposit_Reset_Pass();
            }
            if (next == ST_DEPOSIT_SCAN) {
                Deposit_Reset_Pass();
            }
            if (next == ST_DEPOSIT_DONE) {
                deposit_led_blink_count = 0;
                deposit_led_on = 1;
                Kobuki_Update_LEDs(OFF, GREEN);
            }
            break;
        case ST_DEPOSIT_ALIGN:
            Kobuki_Drive(STOP);
            Kobuki_Rotate_Angle_Setpoint(
                robot.kobuki.angle,
                (int16_t)(Deposit_Angle_Difference(deposit_bearing_tick, robot.kobuki.angle) / 100)
            );
            break;
        case ST_DEPOSIT_MOVE:
            Kobuki_Drive_Distance_Setpoint(
                robot.kobuki.distance,
                deposit_move_mm
            );
            break;
        case ST_ORIENT:
            Kobuki_Rotate_Angle_Setpoint(
                robot.kobuki.angle,
                Layout_Pick(ORIENT_A_DEG, ORIENT_B_DEG)
            );
            break;
        case ST_REVERSE:
        case ST_CORRIDOR_REVERSE:
            Kobuki_Drive_Distance_Setpoint(
                robot.kobuki.distance,
                -PATHFIND_REVERSE_MM
            );
            break;
        case ST_ROTATE:
            Kobuki_Rotate_Angle_Setpoint(
                robot.kobuki.angle,
                robot.pathfind_recover_deg
            );
            break;
        case ST_CORRIDOR_TURN:
            Kobuki_Rotate_Angle_Setpoint(
                robot.kobuki.angle,
                Layout_Pick(PATHFIND_CORNER_A_DEG, PATHFIND_CORNER_B_DEG)
            );
            break;
        case ST_TURN:
            Kobuki_Rotate_Angle_Setpoint(
                robot.kobuki.angle,
                Layout_Pick(STORAGE_TURN_A_DEG, STORAGE_TURN_B_DEG)
            );
            break;
        case ST_DRIVE_IN:
            Kobuki_Drive_Distance_Setpoint(
                robot.kobuki.distance,
                STORAGE_DRIVE_IN_MM
            );
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
            // =========================
            // IDLE
            // =========================
            case ST_IDLE:
                if ((robot.kobuki.button & BUTTON_B0) != 0) {
                    change_to(ST_SEARCH);
                    break;
                }
                Kobuki_Drive(STOP);
                break;
            // =========================
            // SEARCH PHASE
            // =========================
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
                Kobuki_Rotate(SCAN_SPEED_MM_S);
                break;
            case ST_IDENTIFY:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                robot.layout = Layout_From_Range(robot.object.range_mm);
                // Flash LED 1 three times to indicate detected layout
                for (int i = 0; i < 3; i++) {
                    if (robot.layout == LAYOUT_A) {
                        Kobuki_Update_LEDs(RED, OFF);
                    } else {
                        Kobuki_Update_LEDs(YELLOW, OFF);
                    }
                    Delay_ms(500);
                    Kobuki_Update_LEDs(OFF, OFF);
                    Delay_ms(500);
                }
                change_to(ST_ORIENT);
                break;
            // =========================
            // PATHFIND PHASE
            // =========================
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
            // =========================
            // ROTATION STATES
            // =========================
            case ST_ORIENT:
            case ST_ROTATE:
            case ST_CORRIDOR_TURN:
            case ST_TURN:
                if (
                    (state == ST_ORIENT && Fault_Any()) ||
                    (state != ST_ORIENT && Fault_B1_Wheeldrop())
                ) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete == 0) {
                    Kobuki_Rotate_Angle(robot.kobuki.angle);
                    break;
                }
                if (state == ST_ORIENT) {
                    robot.heading = robot.kobuki.angle;
                    robot.corridor = 1;
                    change_to(ST_DRIVE);
                } else if (state == ST_ROTATE) {
                    change_to(ST_DRIVE);
                } else if (state == ST_CORRIDOR_TURN) {
                    robot.corridor = 2;
                    change_to(ST_DRIVE);
                } else {
                    change_to(ST_DRIVE_IN);
                }
                break;
            // =========================
            // STORAGE PHASE
            // =========================
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
            // =========================
            // DEPOSIT PHASE
            // =========================
            case ST_DEPOSIT:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                change_to(ST_DEPOSIT_SCAN);
                break;
            // =========================
            // DEPOSIT SCANNING
            // =========================
            case ST_DEPOSIT_SCAN:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                Scan_Update_Turn(robot.kobuki.angle);
                robot.object.detected = 0;
                Ultrasonic_Detect(&robot.object);
                // Record closest detected object
                if (
                    robot.object.detected != 0 &&
                    robot.object.range_mm >= DEPOSIT_MIN_RANGE_MM &&
                    robot.object.range_mm <= DEPOSIT_MAX_RANGE_MM &&
                    robot.object.range_mm < deposit_best_range
                ) {
                    deposit_best_range = robot.object.range_mm;
                    deposit_candidate_tick = robot.kobuki.angle;
                }
                // Finished one rotation
                if (Scan_Gave_Up()) {
                    if (deposit_best_range == UINT16_MAX) {
                        change_to(ST_FAULT);
                        break;
                    }
                    if (deposit_passes == 0) {
                        deposit_bearing_tick = deposit_candidate_tick;
                        deposit_hits = 1;
                    } else {
                        int16_t diff = Deposit_Angle_Difference(deposit_bearing_tick, deposit_candidate_tick);
                        if (
                            diff >= -(DEPOSIT_BEARING_TOL_DEG * 100) &&
                            diff <= (DEPOSIT_BEARING_TOL_DEG * 100)
                        ) {
                            deposit_hits++;
                        }
                    }
                    deposit_passes++;
                    Kobuki_Drive(STOP);
                    // All scanning passes completed
                    if (deposit_passes >= DEPOSIT_SCAN_PASSES) {
                        if (deposit_hits == DEPOSIT_SCAN_PASSES) {
                            change_to(ST_DEPOSIT_ALIGN);
                        } else {
                            change_to(ST_FAULT);
                        }
                    } else {
                        Deposit_Reset_Pass();
                    }
                    break;
                }
                Kobuki_Rotate(DEPOSIT_SCAN_SPEED_MM_S);
                break;
            // =========================
            // DEPOSIT ALIGNMENT
            // =========================
            case ST_DEPOSIT_ALIGN:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(ST_DEPOSIT_ADJUST);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;
            // =========================
            // DISTANCE ADJUSTMENT
            // =========================
            case ST_DEPOSIT_ADJUST: {
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                robot.object.detected = 0;
                Ultrasonic_Detect(&robot.object);
                // Stop if ultrasonic target is lost
                if (
                    robot.object.detected == 0 ||
                    robot.object.range_mm < DEPOSIT_MIN_RANGE_MM ||
                    robot.object.range_mm > DEPOSIT_MAX_RANGE_MM
                ) {
                    change_to(ST_FAULT);
                    break;
                }
                int32_t error_mm = (int32_t)robot.object.range_mm - DEPOSIT_TARGET_MM;
                // Target distance reached
                if (
                    error_mm >= -DEPOSIT_TOL_MM &&
                    error_mm <= DEPOSIT_TOL_MM
                ) {
                    change_to(ST_DEPOSIT_DONE);
                    break;
                }
                // Prevent unlimited movement attempts
                if (deposit_adjustments >= DEPOSIT_MAX_ADJUSTMENTS) {
                    change_to(ST_FAULT);
                    break;
                }
                // Limit movement distance
                if (error_mm > DEPOSIT_STEP_MM)
                    error_mm = DEPOSIT_STEP_MM;
                if (error_mm < -DEPOSIT_STEP_MM)
                    error_mm = -DEPOSIT_STEP_MM;
                deposit_move_mm = (int16_t)error_mm;
                deposit_adjustments++;
                change_to(ST_DEPOSIT_MOVE);
                break;
            }
            // =========================
            // MOVE TOWARD DEPOSIT
            // =========================
            case ST_DEPOSIT_MOVE:
                if (Fault_Any()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(ST_DEPOSIT_ADJUST);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;
            // =========================
            // TARGET REACHED
            // =========================
            case ST_DEPOSIT_DONE:
                // Stop robot
                Kobuki_Drive(STOP);
                // Blink LED 2 green continuously
                if (++deposit_led_blink_count >= DEPOSIT_LED_BLINK_LOOPS) {
                    deposit_led_blink_count = 0;
                    deposit_led_on = !deposit_led_on;
                    Kobuki_Update_LEDs(OFF, deposit_led_on ? GREEN : OFF);
                }
                if (Fault_B1_Wheeldrop()) {
                    change_to(ST_FAULT);
                }
                break;
            // =========================
            // FAULT STATE
            // =========================
            case ST_FAULT:
                change_to(ST_IDLE);
                break;
        }
    }
}