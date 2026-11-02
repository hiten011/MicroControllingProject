#ifndef HELPER_H
#define HELPER_H

#include <stdbool.h>
#include <stdint.h>
#include "kobuki.h"
#include "ultrasonic.h"

// Kobuki button bits: B0 starts a run, B1 is the emergency stop.
#define KOBUKI_BUTTON_B0_MASK 0x01U
#define KOBUKI_BUTTON_B1_MASK 0x02U

// Bumper and cliff side bits.
#define KOBUKI_SIDE_RIGHT_MASK  0x01U
#define KOBUKI_SIDE_CENTRE_MASK 0x02U
#define KOBUKI_SIDE_LEFT_MASK   0x04U
#define KOBUKI_SIDE_ALL_MASK    (KOBUKI_SIDE_RIGHT_MASK | KOBUKI_SIDE_CENTRE_MASK | KOBUKI_SIDE_LEFT_MASK)

// Search spin: give up after a full turn (degrees from the heading at B0, anticlockwise positive).
#define SCAN_FULL_TURN_DEG     360
#define SCAN_ROTATE_SPEED_MM_S 150

// The first reading on a bollard is under this for Layout B (the nearer bollard), otherwise Layout A.
#define LAYOUT_B_MAX_RANGE_MM 575U

// Orient: Layout A turns CW to the bottom corridor; Layout B turns ACW to the left corridor.
#define ORIENT_TURN_LAYOUT_A_DEG (-75)
#define ORIENT_TURN_LAYOUT_B_DEG (108)

// Pathfind: drive speed, corridor corner turns, and the reverse-then-turn recovery after a wall or ledge hit.
#define PATHFIND_DRIVE_SPEED_MM_S          150
#define PATHFIND_CORNER_TURN_LAYOUT_A_DEG  90
#define PATHFIND_CORNER_TURN_LAYOUT_B_DEG  (-90)
#define PATHFIND_REVERSE_MM                50
#define PATHFIND_RECOVER_TURN_LAYOUT_A_DEG (-5)
#define PATHFIND_RECOVER_TURN_LAYOUT_B_DEG 5

// Storage: turn to face the storage unit, then drive in.
#define STORAGE_TURN_LAYOUT_A_DEG 131
#define STORAGE_TURN_LAYOUT_B_DEG (-131)
#define STORAGE_DRIVE_IN_MM       900

// Which test-area layout was identified.
typedef enum { LAYOUT_NONE, LAYOUT_A, LAYOUT_B } Layout;

// All run-time state shared between main.c and the helper functions.
typedef struct {
    Kobuki_Typedef kobuki;
    US_Typedef object;
    Layout layout;
    int16_t heading;
    uint8_t corridor;              // 1 = first corridor, 2 = second
    int16_t pathfind_recover_deg;  // turn to apply after the Reverse
    int16_t scan_prev_angle;       // last gyro reading, for the wrap-safe turn sum
    int32_t scan_ticks;            // total turn in 0.01 deg gyro ticks
    int16_t scan_deg;              // total turn in degrees
} RobotData;

// The one global RobotData instance.
extern RobotData robot;

// B1 pressed or a wheel dropped.
bool Fault_B1_Wheeldrop(void);

// B1, wheeldrop, any bumper or any cliff.
bool Fault_Any(void);

// Layout B when range_mm is under LAYOUT_B_MAX_RANGE_MM, otherwise Layout A.
Layout Layout_From_Range(uint16_t range_mm);

// a_deg when the Layout is A, otherwise b_deg.
int16_t Layout_Pick(int16_t a_deg, int16_t b_deg);

// Zero the scan angle. Call when a run starts.
void Scan_Start(int16_t angle_tick);

// Add the turn since the last call to scan_deg. Gyro ticks are 0.01 deg and wrap at +-18000.
void Scan_Update_Turn(int16_t angle_tick);

// Update scan_deg from the gyro and read the ultrasonic once.
void Scan_Sample(void);

// True once the AMU has turned a full circle without finding anything.
bool Scan_Gave_Up(void);

// Storage-wall bumper in Drive. Latches the nudge away from the wall into pathfind_recover_deg.
bool Corridor_Take_Wall_Hit(uint8_t bumper);

// Ledge cliff in Drive. Latches the nudge away from the ledge into pathfind_recover_deg.
bool Corridor_Take_Ledge_Hit(const Kobuki_Typedef *kobuki);

#endif
