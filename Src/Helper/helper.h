#ifndef HELPER_H
#define HELPER_H

#include <stdint.h>
#include <stdbool.h>

#include "kobuki.h"
#include "ultrasonic.h"

// Buttons
#define BUTTON_B0 0x01U
#define BUTTON_B1 0x02U

// Sensor positions
#define SIDE_RIGHT  0x01U
#define SIDE_CENTRE 0x02U
#define SIDE_LEFT   0x04U

// Search
#define SPIN_STEP_DEG 15

// Layout detection
#define LAYOUT_SPLIT_MM 500U

// Alignment
#define ALIGNMENT_OFFSET_DEG 13

// Orientation
#define ORIENT_A_DEG (-90)
#define ORIENT_B_DEG 90

// Pathfinding
#define PATHFIND_DRIVE_SPEED 100

#define PATHFIND_CORRIDOR_1_A_MM 1200
#define PATHFIND_CORRIDOR_2_A_MM 1400

#define PATHFIND_CORRIDOR_1_B_MM 1400
#define PATHFIND_CORRIDOR_2_B_MM 1200

#define PATHFIND_CORNER_A_DEG 90
#define PATHFIND_CORNER_B_DEG (-90)

#define PATHFIND_REVERSE_MM 50

#define PATHFIND_ROTATE_A_DEG 10
#define PATHFIND_ROTATE_B_DEG (-10)

// Storage
#define STORAGE_TURN_A_DEG 135
#define STORAGE_TURN_B_DEG (-135)

#define STORAGE_DRIVE_IN_MM 1000

// Layout
typedef enum {
    LAYOUT_NONE,
    LAYOUT_A,
    LAYOUT_B
} Layout;

// Shared robot data
typedef struct {
    Kobuki_Typedef kobuki;
    US_Typedef object;

    Layout layout;

    int16_t heading;
    uint8_t corridor;
    uint16_t corridor_start_distance;

} RobotData;

extern RobotData robot;

// Layout detection
Layout Layout_From_Range(uint16_t range_mm);

// Robot initialisation
void Robot_Init(void);

// Sensor readings
void Robot_ReadPacket(void);
uint16_t Robot_ReadUltrasonic(void);
bool Robot_BumperPressed(void);

// Movement control
void Robot_Drive(int16_t speed_mm_s);

void Robot_Move(int distance_mm, int speed_mm_s);

void Robot_Rotate(int angle_deg, int speed_mm_s);

void Robot_Update(void);

bool Robot_MovementComplete(void);

void Robot_Stop(void);

#endif