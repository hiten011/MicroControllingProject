#include <stdint.h>
#include <stdbool.h>

#define STM32G474xx

#include "stm32g4xx.h"
#include "uart.h"
#include "helper.h"


// --------------------------------------------------
// FSM STATES
// --------------------------------------------------

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


// --------------------------------------------------
// FAULT DETECTION
// --------------------------------------------------

static bool b1_or_wheeldrop(void)
{
    return (robot.kobuki.button & BUTTON_B1) != 0 ||
           robot.kobuki.wheeldrop != 0;
}

static bool fault_all(void)
{
    return b1_or_wheeldrop() ||
           robot.kobuki.bumper != 0 ||
           robot.kobuki.cliff != 0;
}

static bool fault_ignore_cliff(void)
{
    return b1_or_wheeldrop() ||
           robot.kobuki.bumper != 0;
}


// --------------------------------------------------
// CORRIDOR LENGTH
// --------------------------------------------------

static int16_t corridor_length_mm(void)
{
    if (robot.layout == LAYOUT_A) {

        return (robot.corridor == 1)
            ? PATHFIND_CORRIDOR_1_A_MM
            : PATHFIND_CORRIDOR_2_A_MM;
    }

    return (robot.corridor == 1)
        ? PATHFIND_CORRIDOR_1_B_MM
        : PATHFIND_CORRIDOR_2_B_MM;
}


// --------------------------------------------------
// STATE TRANSITIONS
// --------------------------------------------------

static void change_to(State next)
{
    state = next;

    switch (next) {

        case ST_DETECT:
            Robot_Stop();
            Robot_ReadUltrasonic();
            break;

        case ST_SPIN:
            Robot_Rotate(SPIN_STEP_DEG, 0);
            break;

        case ST_ALIGN:
            Robot_Rotate(ALIGNMENT_OFFSET_DEG, 0);
            break;

        case ST_ORIENT: {

            int16_t deg = (robot.layout == LAYOUT_A)
                ? ORIENT_A_DEG
                : ORIENT_B_DEG;

            Robot_Rotate(deg, 0);
            break;
        }

        case ST_PATHFIND_REVERSE:
            Robot_Move(-PATHFIND_REVERSE_MM, 0);
            break;

        case ST_PATHFIND_ROTATE: {

            int16_t deg = (robot.layout == LAYOUT_A)
                ? PATHFIND_ROTATE_A_DEG
                : PATHFIND_ROTATE_B_DEG;

            Robot_Rotate(deg, 0);
            break;
        }

        case ST_PATHFIND_CORRIDOR_TURN: {

            int16_t deg = (robot.layout == LAYOUT_A)
                ? PATHFIND_CORNER_A_DEG
                : PATHFIND_CORNER_B_DEG;

            Robot_Rotate(deg, 0);
            break;
        }

        case ST_PATHFIND_CORRIDOR_STOP:
            Robot_Stop();
            break;

        case ST_STORAGE_TURN: {

            int16_t deg = (robot.layout == LAYOUT_A)
                ? STORAGE_TURN_A_DEG
                : STORAGE_TURN_B_DEG;

            Robot_Rotate(deg, 0);
            break;
        }

        case ST_STORAGE_DRIVE_IN:
            Robot_Move(STORAGE_DRIVE_IN_MM, 0);
            break;

        case ST_STORAGE_DEPOSIT:
            Robot_Stop();
            break;

        case ST_FAULT:
            Robot_Stop();
            break;

        case ST_SEARCH:
        case ST_IDLE:
        case ST_PATHFIND_DRIVE:
            break;
    }
}


// --------------------------------------------------
// SEARCH DECISION
// --------------------------------------------------

typedef enum {
    SEARCH_KEEP_ROTATING,
    SEARCH_ALIGN_A,
    SEARCH_ALIGN_B,
    SEARCH_FAULT_LAYOUT
} SearchDecision;

static SearchDecision Search_Decide(const US_Typedef *object)
{
    if (object->detected == 0) {
        return SEARCH_KEEP_ROTATING;
    }

    switch (Layout_From_Range(object->range_mm)) {

        case LAYOUT_A:
            return SEARCH_ALIGN_A;

        case LAYOUT_B:
            return SEARCH_ALIGN_B;

        case LAYOUT_NONE:
            return SEARCH_FAULT_LAYOUT;
    }

    return SEARCH_FAULT_LAYOUT;
}


// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main(void)
{
    UART_Init();
    Robot_Init();

    while (1) {

        Robot_ReadPacket();

        switch (state) {

            // --------------------------------------
            // IDLE
            // --------------------------------------

            case ST_IDLE:

                if ((robot.kobuki.button & BUTTON_B0) != 0) {

                    robot.layout = LAYOUT_NONE;
                    robot.object.detected = 0;
                    robot.heading = 0;
                    robot.corridor = 0;

                    change_to(ST_SEARCH);
                    break;
                }

                Robot_Stop();
                break;


            // --------------------------------------
            // SEARCH
            // --------------------------------------

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


            // --------------------------------------
            // DETECT
            // --------------------------------------

            case ST_DETECT:

                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (Search_Decide(&robot.object) ==
                    SEARCH_KEEP_ROTATING) {

                    change_to(ST_SPIN);
                    break;
                }

                change_to(ST_SEARCH);
                break;


            // --------------------------------------
            // SPIN
            // --------------------------------------

            case ST_SPIN:

                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {
                    change_to(ST_SEARCH);
                }

                break;


            // --------------------------------------
            // ALIGN
            // --------------------------------------

            case ST_ALIGN:

                if (fault_ignore_cliff()) {
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {
                    change_to(ST_ORIENT);
                }

                break;


            // --------------------------------------
            // ORIENT
            // --------------------------------------

            case ST_ORIENT:

                if (fault_all()) {
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {

                    robot.heading = robot.kobuki.angle;
                    robot.corridor = 1;

                    robot.corridor_start_distance =
                        robot.kobuki.distance;

                    change_to(ST_PATHFIND_DRIVE);
                }

                break;


            // --------------------------------------
            // PATHFIND DRIVE
            // --------------------------------------

            case ST_PATHFIND_DRIVE: {

                bool corridor_done =
                    Kobuki_Distance_Delta_Mm(
                        robot.corridor_start_distance,
                        robot.kobuki.distance
                    ) >= corridor_length_mm();

                uint8_t wall_mask =
                    (robot.layout == LAYOUT_A)
                    ? SIDE_RIGHT
                    : SIDE_LEFT;

                uint8_t ledge_mask =
                    (robot.layout == LAYOUT_A)
                    ? SIDE_LEFT
                    : SIDE_RIGHT;

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

                if ((robot.kobuki.bumper & wall_mask) != 0 ||
                    (robot.kobuki.cliff & ledge_mask) != 0) {

                    change_to(ST_PATHFIND_REVERSE);
                    break;
                }

                Robot_Drive(PATHFIND_DRIVE_SPEED);
                break;
            }


            // --------------------------------------
            // PATHFIND REVERSE
            // --------------------------------------

            case ST_PATHFIND_REVERSE:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {
                    change_to(ST_PATHFIND_ROTATE);
                }

                break;


            // --------------------------------------
            // PATHFIND ROTATE
            // --------------------------------------

            case ST_PATHFIND_ROTATE:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {
                    change_to(ST_PATHFIND_DRIVE);
                }

                break;


            // --------------------------------------
            // CORRIDOR TURN
            // --------------------------------------

            case ST_PATHFIND_CORRIDOR_TURN:

                if (b1_or_wheeldrop()) {
                    robot.heading = robot.kobuki.angle;
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {

                    robot.heading = robot.kobuki.angle;
                    robot.corridor = 2;

                    robot.corridor_start_distance =
                        robot.kobuki.distance;

                    change_to(ST_PATHFIND_DRIVE);
                }

                break;


            // --------------------------------------
            // CORRIDOR STOP
            // --------------------------------------

            case ST_PATHFIND_CORRIDOR_STOP:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                change_to(ST_STORAGE_TURN);
                break;


            // --------------------------------------
            // STORAGE TURN
            // --------------------------------------

            case ST_STORAGE_TURN:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {
                    change_to(ST_STORAGE_DRIVE_IN);
                }

                break;


            // --------------------------------------
            // STORAGE DRIVE
            // --------------------------------------

            case ST_STORAGE_DRIVE_IN:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                if (Robot_BumperPressed()) {

                    Robot_Stop();
                    change_to(ST_STORAGE_DEPOSIT);
                    break;
                }

                Robot_Update();

                if (Robot_MovementComplete()) {
                    change_to(ST_STORAGE_DEPOSIT);
                }

                break;


            // --------------------------------------
            // STORAGE DEPOSIT
            // --------------------------------------

            case ST_STORAGE_DEPOSIT:

                if (b1_or_wheeldrop()) {
                    change_to(ST_FAULT);
                    break;
                }

                change_to(ST_IDLE);
                break;


            // --------------------------------------
            // FAULT
            // --------------------------------------

            case ST_FAULT:

                change_to(ST_IDLE);
                break;
        }
    }
}