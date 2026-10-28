#include "robot_control.h"
#include "adc.h"
#include "ultrasonic.h"

typedef enum { ROBOT_NONE, ROBOT_LINEAR, ROBOT_TURN } Robot_Command;

static Robot_Command current_command = ROBOT_NONE;
static bool moving = false;
static bool bumper_latched = false;
static bool bumper_pressed_now = false;
static int target_value = 0;
static bool command_pending = false;

void Robot_Stop(void)
{
    Kobuki_Drive(STOP);
    current_command = ROBOT_NONE;
    command_pending = false;
    moving = false;
}

void Robot_Init(void)
{
    bumper_latched = false;
    bumper_pressed_now = false;
    target_value = 0;

    ADC_Init();
    Ultrasonic_Init();

    Robot_Stop();
}

void Robot_Move(int distance_mm, int speed_mm_s)
{
    // Driver distance helpers select motor speed internally.
    // The requested speed is reserved until speed-aware driver support exists.
    (void)speed_mm_s;

    if (bumper_pressed_now || distance_mm == 0) {
        Robot_Stop();
        return;
    }

    Robot_Stop();

    bumper_latched = false;
    target_value = distance_mm;
    current_command = ROBOT_LINEAR;
    command_pending = true;
    moving = true;
}

void Robot_Rotate(int angle_deg, int speed_mm_s)
{
    // Driver rotation helpers select motor speed internally.
    (void)speed_mm_s;

    if (bumper_pressed_now || angle_deg == 0) {
        Robot_Stop();
        return;
    }

    Robot_Stop();

    bumper_latched = false;
    target_value = angle_deg;
    current_command = ROBOT_TURN;
    command_pending = true;
    moving = true;
}

bool Robot_MovementComplete(void)
{
    return !moving;
}

bool Robot_BumperPressed(void)
{
    return bumper_pressed_now;
}

Robot_Status Robot_Update(Kobuki_Typedef *kobuki)
{
    Kobuki_Read(kobuki);

    bumper_pressed_now = (kobuki->bumper != 0b000);

    // Stop movement if any bumper is pressed.
    if (bumper_pressed_now) {
        bumper_latched = true;
        Robot_Stop();
    }

    if (moving) {

        // Set the movement target.
        if (command_pending) {

            if (current_command == ROBOT_LINEAR) {
                Kobuki_Drive_Distance_Setpoint(kobuki->distance, target_value);
            } else if (current_command == ROBOT_TURN) {
                Kobuki_Rotate_Angle_Setpoint(kobuki->angle, target_value);
            }

            command_pending = false;
        }

        // Check whether movement is complete.
        else if ((current_command == ROBOT_LINEAR && kobuki->distance_complete) ||

                 (current_command == ROBOT_TURN && kobuki->rotation_complete)) {
            Robot_Stop();
        }

        // Continue moving.
        else if (current_command == ROBOT_LINEAR) {
            Kobuki_Drive_Distance(kobuki->distance);
        }

        // Continue rotating.
        else if (current_command == ROBOT_TURN) {
            Kobuki_Rotate_Angle(kobuki->angle);
        }
    } else {
        Kobuki_Drive(STOP);
    }

    Robot_Status status = {.moving = moving, .bumper_hit = bumper_latched, .bumper_bits = kobuki->bumper};

    return status;
}

// Read the ultrasonic sensor distance in millimetres.
uint16_t Robot_ReadUltrasonic(void)
{
    return Ultrasonic_Read();
}