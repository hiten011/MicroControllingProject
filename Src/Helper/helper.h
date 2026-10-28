#ifndef HELPER_H
#define HELPER_H

#include <stdbool.h>
#include <stdint.h>
#include "kobuki.h"
#include "ultrasonic.h"

#define BUTTON_B0 0x01U
#define BUTTON_B1 0x02U

#define SIDE_RIGHT  0x01U
#define SIDE_CENTRE 0x02U
#define SIDE_LEFT   0x04U
#define SIDE_ANY    (SIDE_RIGHT | SIDE_CENTRE | SIDE_LEFT)

// Scan angles are degrees from the heading at B0, anticlockwise positive.
#define SCAN_FULL_DEG   360
#define SCAN_SPEED_MM_S 100
#define LOST_DEG        10
#define GIVE_UP_DEG     40
#define NO_ANGLE        10000

// Edge scan turns back clockwise across the bollard to find where it starts and ends.
#define EDGE_SCAN_SPEED_MM_S 40
#define EDGE_BAND_MM         60U

// The closest reading on a bollard is under this for Layout B (the nearer bollard), otherwise Layout A.
#define LAYOUT_B_MAX_MM 575U

// Orient: Layout A turns CW to the bottom corridor; Layout B turns ACW to the left corridor.
#define ORIENT_A_DEG (-90)
#define ORIENT_B_DEG (90)

#define PATHFIND_DRIVE_SPEED 100
#define PATHFIND_CORNER_A_DEG    90
#define PATHFIND_CORNER_B_DEG    (-100)
#define PATHFIND_REVERSE_MM      50
#define PATHFIND_ROTATE_A_DEG    (-5)
#define PATHFIND_ROTATE_B_DEG    5

// Figure 2 centreline legs. A is bottom 1200 then right 1400. B is left 1400 then top 1200.
#define PATHFIND_CORRIDOR_1_A_MM 950
#define PATHFIND_CORRIDOR_2_A_MM 1550
#define PATHFIND_CORRIDOR_1_B_MM 1550
#define PATHFIND_CORRIDOR_2_B_MM 950

#define STORAGE_TURN_A_DEG  135
#define STORAGE_TURN_B_DEG  (-135)
#define STORAGE_DRIVE_IN_MM 1000

typedef enum { LAYOUT_NONE, LAYOUT_A, LAYOUT_B } Layout;

typedef struct {
    Kobuki_Typedef kobuki;
    US_Typedef object;
    Layout layout;
    int16_t heading;
    uint8_t corridor;
    uint16_t corridor_start_tick;
    int16_t pathfind_recover_deg;
    int16_t scan_prev_angle;
    int32_t scan_ticks;
    int16_t scan_deg;
    int16_t first_seen_deg;
    int16_t last_seen_deg;
    uint16_t closest_mm;
    uint16_t edge_mm;
    int16_t edge_start_deg;
    int16_t edge_end_deg;
    int16_t centre_deg;
} RobotData;

extern RobotData robot;

// Layout B when the closest bollard reading is under LAYOUT_B_MAX_MM, otherwise Layout A.
Layout Layout_From_Range(uint16_t closest_mm);

// Reset the layout, object, heading, corridor and scan state. Call when a run starts.
void Robot_Start(int16_t angle_tick);

// Recovery must not call this. Returning to Drive keeps the original start tick.
void Corridor_Begin(uint8_t index);

int16_t Corridor_Length_Mm(void);

bool Corridor_Take_Wall_Hit(uint8_t bumper);

bool Corridor_Take_Ledge_Hit(const Kobuki_Typedef *kobuki);

// Add the turn since the last call to scan_deg. Gyro ticks are 0.01 deg and wrap at +-18000.
void Scan_Update_Turn(int16_t angle_tick);

#endif
