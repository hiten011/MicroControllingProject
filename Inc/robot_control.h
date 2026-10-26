#ifndef ROBOT_CONTROL_H
#define ROBOT_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "kobuki.h"

typedef struct {
    bool moving;
    bool bumper_hit;
    uint8_t bumper_bits;
} Robot_Status;

// Call once after UART_Init().
void Robot_Init(void);

// Set a new motion target (signed distance in mm or signed angle in degrees).
// Commands are nonblocking: keep calling Robot_Update() in the main loop.
// Positive distance moves forward; negative distance moves backward.
// Positive/negative rotation follows the Kobuki driver's angle convention.
// NOTE: The current Kobuki distance/rotation helpers do not expose speed;
// speed_mm_s is reserved for future driver support and is not yet applied.
void Robot_Move(int distance_mm, int speed_mm_s);
void Robot_Rotate(int angle_deg, int speed_mm_s);

// Read sensors, enforce bumper stops, and progress the active command.
Robot_Status Robot_Update(Kobuki_Typedef *kobuki);

// Cancel the active command and stop the motors.
void Robot_Stop(void);

// Returns true when there is no active movement command.
bool Robot_MovementComplete(void);

// Returns true when the most recent bumper reading was nonzero.
bool Robot_BumperPressed(void);
// Read the ultrasonic sensor and return distance in millimetres.
uint16_t Robot_ReadUltrasonic(void);

#endif