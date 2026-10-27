#include "helper.h"
#include "adc.h"

// Shared robot data
RobotData robot;

// Movement command types
typedef enum {
    ROBOT_CMD_NONE,
    ROBOT_CMD_MOVE,
    ROBOT_CMD_ROTATE
} RobotCommand;

// Current movement
static RobotCommand command = ROBOT_CMD_NONE;

static bool command_complete = true;


// --------------------------------------------------
// INITIALISATION
// --------------------------------------------------

void Robot_Init(void)
{
    ADC_Init();
    Ultrasonic_Init();

    Robot_Stop();
}


// --------------------------------------------------
// SENSOR FUNCTIONS
// --------------------------------------------------

void Robot_ReadPacket(void)
{
    Kobuki_Read(&robot.kobuki);
}

uint16_t Robot_ReadUltrasonic(void)
{
    Ultrasonic_Detect(&robot.object);

    return robot.object.range_mm;
}

bool Robot_BumperPressed(void)
{
    return robot.kobuki.bumper != 0;
}


// --------------------------------------------------
// LAYOUT DETECTION
// --------------------------------------------------

Layout Layout_From_Range(uint16_t range_mm)
{
    if (range_mm > LAYOUT_SPLIT_MM) {
        return LAYOUT_A;
    }

    return LAYOUT_B;
}


// --------------------------------------------------
// STOP
// --------------------------------------------------

void Robot_Stop(void)
{
    Kobuki_Drive(STOP);

    command = ROBOT_CMD_NONE;
    command_complete = true;
}


// --------------------------------------------------
// CONTINUOUS MOVEMENT
// --------------------------------------------------

void Robot_Drive(int16_t speed_mm_s)
{
    command = ROBOT_CMD_NONE;
    command_complete = true;

    Kobuki_Drive(speed_mm_s);
}


// --------------------------------------------------
// MOVE SPECIFIED DISTANCE
// --------------------------------------------------

void Robot_Move(int distance_mm, int speed_mm_s)
{
    // Existing Kobuki distance controller manages speed.
    (void)speed_mm_s;

    if (distance_mm == 0) {
        Robot_Stop();
        return;
    }

    Kobuki_Drive_Distance_Setpoint(
        robot.kobuki.distance,
        (int16_t)distance_mm
    );

    command = ROBOT_CMD_MOVE;
    command_complete = false;
}


// --------------------------------------------------
// ROTATE SPECIFIED ANGLE
// --------------------------------------------------

void Robot_Rotate(int angle_deg, int speed_mm_s)
{
    // Existing Kobuki rotation controller manages speed.
    (void)speed_mm_s;

    if (angle_deg == 0) {
        Robot_Stop();
        return;
    }

    Kobuki_Rotate_Angle_Setpoint(
        robot.kobuki.angle,
        (int16_t)angle_deg
    );

    command = ROBOT_CMD_ROTATE;
    command_complete = false;
}


// --------------------------------------------------
// UPDATE MOVEMENT
// --------------------------------------------------

void Robot_Update(void)
{
    switch (command) {

        case ROBOT_CMD_MOVE:

            Kobuki_Drive_Distance(
                robot.kobuki.distance
            );

            if (robot.kobuki.distance_complete != 0) {
                Robot_Stop();
            }

            break;

        case ROBOT_CMD_ROTATE:

            Kobuki_Rotate_Angle(
                robot.kobuki.angle
            );

            if (robot.kobuki.rotation_complete != 0) {
                Robot_Stop();
            }

            break;

        case ROBOT_CMD_NONE:
            break;
    }
}


// --------------------------------------------------
// MOVEMENT COMPLETION
// --------------------------------------------------

bool Robot_MovementComplete(void)
{
    return command_complete;
}