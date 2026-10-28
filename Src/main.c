#include <stdint.h>
#include <stdbool.h>

#define STM32G474xx

#include "stm32g4xx.h"
#include "uart.h"
#include "adc.h"
#include "ultrasonic.h"
#include "kobuki.h"
#include "helper.h"

typedef enum {
    ST_IDLE,
    ST_SEARCH,
    ST_DETECT,
    ST_SPIN,
    ST_ALIGN,
    ST_ORIENT,
    ST_PATHFIND_DRIVE,
    ST_PATHFIND_REVERSE,
    ST_PATHFIND_ROTATE,
    ST_PATHFIND_CORRIDOR_TURN,
    ST_PATHFIND_CORRIDOR_STOP,
    ST_STORAGE_TURN,
    ST_STORAGE_DRIVE_IN,
    ST_STORAGE_DEPOSIT,
    ST_FAULT
} State;

static State state = ST_IDLE;

static bool b1_or_wheeldrop(void)
{
    return (robot.kobuki.button & BUTTON_B1) != 0 || robot.kobuki.wheeldrop != 0;
}

static bool fault_all(void)
{
    return b1_or_wheeldrop() || robot.kobuki.bumper != 0 || robot.kobuki.cliff != 0;
}

static bool fault_ignore_cliff(void)
{
    return b1_or_wheeldrop() || robot.kobuki.bumper != 0;
}

static int16_t corridor_length_mm(void)
{
    if (robot.layout == LAYOUT_A) {
        return (robot.corridor == 1) ? PATHFIND_CORRIDOR_1_A_MM : PATHFIND_CORRIDOR_2_A_MM;
    }
    return (robot.corridor == 1) ? PATHFIND_CORRIDOR_1_B_MM : PATHFIND_CORRIDOR_2_B_MM;
}

// Enter a state: run that state's entry work, then leave its do/checks for the next packet.
static void change_to(State next)
{
    state = next;

    switch (next) {
        case ST_DETECT:
            Kobuki_Drive(STOP);
            Ultrasonic_Detect(&robot.object);
            break;

        case ST_SPIN:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, SPIN_STEP_DEG);
            break;

        case ST_ALIGN:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, ALIGNMENT_OFFSET_DEG);
            break;

        case ST_ORIENT: {
            int16_t deg = (robot.layout == LAYOUT_A) ? ORIENT_A_DEG : ORIENT_B_DEG;
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, deg);
            break;
        }

        case ST_PATHFIND_REVERSE:
            Kobuki_Drive_Distance_Setpoint(robot.kobuki.distance, -PATHFIND_REVERSE_MM);
            break;

        case ST_PATHFIND_ROTATE:
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, robot.pathfind_recover_deg);
            break;

        case ST_PATHFIND_CORRIDOR_TURN: {
            int16_t deg = (robot.layout == LAYOUT_A) ? PATHFIND_CORNER_A_DEG : PATHFIND_CORNER_B_DEG;
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, deg);
            break;
        }

        case ST_PATHFIND_CORRIDOR_STOP:
            Kobuki_Drive(STOP);
            break;

        case ST_STORAGE_TURN: {
            int16_t deg = (robot.layout == LAYOUT_A) ? STORAGE_TURN_A_DEG : STORAGE_TURN_B_DEG;
            Kobuki_Rotate_Angle_Setpoint(robot.kobuki.angle, deg);
            break;
        }

        case ST_STORAGE_DRIVE_IN:
            Kobuki_Drive_Distance_Setpoint(robot.kobuki.distance, STORAGE_DRIVE_IN_MM);
            break;

        case ST_STORAGE_DEPOSIT:
            Kobuki_Drive(STOP);
            break;

        case ST_FAULT:
            Kobuki_Drive(STOP);
            break;

        case ST_SEARCH:
        case ST_IDLE:
        case ST_PATHFIND_DRIVE:
            break;
    }
}

static void wait_next_kobuki_packet(void)
{
    Kobuki_Read(&robot.kobuki);
}

// SEARCH_FAULT_LAYOUT stays because later states treat non-A as B.
// A hit stores layout on the SEARCH tick that enters ALIGN. That packet's ALIGN entry arms +13 degrees.
typedef enum {
    SEARCH_KEEP_ROTATING,
    SEARCH_ALIGN_A,
    SEARCH_ALIGN_B,
    SEARCH_FAULT_LAYOUT,
} SearchDecision;

static SearchDecision Search_Decide(const US_Typedef *object)
{
    if (object->detected == 0) {
        return SEARCH_KEEP_ROTATING;
    }
    switch (Layout_From_Range(object->range_mm)) {
        case LAYOUT_A:
            Kobuki_Update_LEDs(RED, Kobuki_LED_Enum NONE);
            return SEARCH_ALIGN_A;
        case LAYOUT_B:
            Kobuki_Update_LEDs(YELLOW, Kobuki_LED_Enum NONE);
            return SEARCH_ALIGN_B;
        case LAYOUT_NONE:
            Kobuki_Update_LEDs(GREEN, Kobuki_LED_Enum NONE);
            return SEARCH_FAULT_LAYOUT;
    }
    // Layout's three cases all return. This keeps cppcheck's missingReturn quiet.
    return SEARCH_FAULT_LAYOUT;
}

int main(void)
{
    UART_Init();
    ADC_Init();
    Ultrasonic_Init();

    while (1) {
        wait_next_kobuki_packet();

        switch (state) {
            case ST_IDLE:
                if ((robot.kobuki.button & BUTTON_B0) != 0) {
                    robot.layout = LAYOUT_NONE;
                    robot.object.detected = 0;
                    robot.heading = 0;
                    robot.corridor = 0;
                    change_to(ST_SEARCH);
                    break;
                }
                Kobuki_Drive(STOP);
                break;

            case ST_SEARCH:
                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }
                switch (Search_Decide(&robot.object)) {
                    case SEARCH_KEEP_ROTATING:
                        change_to(ST_DETECT);
                        break;
                    case SEARCH_FAULT_LAYOUT:
                        change_to(ST_FAULT);
                        break;
                    case SEARCH_ALIGN_A:
                        robot.layout = LAYOUT_A;
                        change_to(ST_ALIGN);
                        break;
                    case SEARCH_ALIGN_B:
                        robot.layout = LAYOUT_B;
                        change_to(ST_ALIGN);
                        break;
                }
                break;

            case ST_DETECT:
                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (Search_Decide(&robot.object) == SEARCH_KEEP_ROTATING) {
                    change_to(ST_SPIN);
                    break;
                }
                change_to(ST_SEARCH);
                break;

            case ST_SPIN:
                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(ST_SEARCH);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            case ST_ALIGN:
                if (fault_ignore_cliff()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(ST_ORIENT);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            case ST_ORIENT:
                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    robot.heading = robot.kobuki.angle;
                    robot.corridor = 1;
                    robot.corridor_start_distance = robot.kobuki.distance;
                    change_to(ST_PATHFIND_DRIVE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            case ST_PATHFIND_DRIVE: {
                bool corridor_done = Kobuki_Distance_Delta_Mm(robot.corridor_start_distance, robot.kobuki.distance) >=
                                     corridor_length_mm();
                uint8_t wall_mask = (robot.layout == LAYOUT_A) ? SIDE_LEFT : SIDE_RIGHT;
                uint8_t ledge_mask = (robot.layout == LAYOUT_A) ? SIDE_RIGHT : SIDE_LEFT;
                int16_t wall_nudge = (robot.layout == LAYOUT_A) ? PATHFIND_ROTATE_A_DEG : PATHFIND_ROTATE_B_DEG;

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (corridor_done && robot.corridor == 1) {
                    change_to(ST_PATHFIND_CORRIDOR_TURN);
                    break;
                }
                if (corridor_done && robot.corridor == 2) {
                    change_to(ST_PATHFIND_CORRIDOR_STOP);
                    break;
                }
                if ((robot.kobuki.bumper & wall_mask) != 0) {
                    robot.pathfind_recover_deg = wall_nudge;
                    change_to(ST_PATHFIND_REVERSE);
                    break;
                }
                if ((robot.kobuki.cliff & ledge_mask) != 0) {
                    robot.pathfind_recover_deg = (int16_t)(-wall_nudge);
                    change_to(ST_PATHFIND_REVERSE);
                    break;
                }
                Kobuki_Drive(PATHFIND_DRIVE_SPEED);
                break;
            }

            case ST_PATHFIND_REVERSE:
                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(ST_PATHFIND_ROTATE);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            case ST_PATHFIND_ROTATE:
                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(ST_PATHFIND_DRIVE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            case ST_PATHFIND_CORRIDOR_TURN:
                if (b1_or_wheeldrop()) {
                    robot.heading = robot.kobuki.angle;
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    robot.heading = robot.kobuki.angle;
                    robot.corridor = 2;
                    robot.corridor_start_distance = robot.kobuki.distance;
                    change_to(ST_PATHFIND_DRIVE);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            case ST_PATHFIND_CORRIDOR_STOP:
                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                change_to(ST_STORAGE_TURN);
                break;

            case ST_STORAGE_TURN:
                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.rotation_complete != 0) {
                    change_to(ST_STORAGE_DRIVE_IN);
                    break;
                }
                Kobuki_Rotate_Angle(robot.kobuki.angle);
                break;

            case ST_STORAGE_DRIVE_IN:
                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                if (robot.kobuki.bumper != 0) {
                    Kobuki_Drive(STOP);
                    change_to(ST_STORAGE_DEPOSIT);
                    break;
                }
                if (robot.kobuki.distance_complete != 0) {
                    change_to(ST_STORAGE_DEPOSIT);
                    break;
                }
                Kobuki_Drive_Distance(robot.kobuki.distance);
                break;

            case ST_STORAGE_DEPOSIT:
                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }
                change_to(ST_IDLE);
                break;

            case ST_FAULT:
                change_to(ST_IDLE);
                break;
        }
    }
}
