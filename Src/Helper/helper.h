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

// Scan spin: stop after a full turn (degrees from the heading at the start of each scan, anticlockwise positive).
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

// Start directly in Deposit for bench testing. Leave at 0 for the full parcel run.
#ifndef DEPOSIT_TEST_MODE
#define DEPOSIT_TEST_MODE 0
#endif

// Deposit scan and align: slow enough for the ultrasonic's 10 Hz output while turning anticlockwise.
#define DEPOSIT_SCAN_SPEED_MM_S 50

// Deposit align: stop facing the saved measurement within 1 degree (gyro ticks are 0.01 degree).
#define DEPOSIT_ALIGN_TOLERANCE_TICKS 100

// Deposit distance: target and tolerance, measured from the ultrasonic sensor face to the rod.
// Reverse also backs off by an extra margin so the final correction is made moving forward.
#define DEPOSIT_TARGET_RANGE_MM     410U
#define DEPOSIT_RANGE_TOLERANCE_MM  10U
#define DEPOSIT_REVERSE_MARGIN_MM   100U
#define DEPOSIT_REVERSE_SPEED_MM_S  50
#define DEPOSIT_APPROACH_SPEED_MM_S 35

// Hold still for about 300 ms at the Kobuki's normal 50 Hz feedback rate.
// This lets the 10 Hz ultrasonic output catch up after turning or stopping.
#define DEPOSIT_SETTLE_PACKETS 15U

// Which test-area layout was identified.
typedef enum { LAYOUT_NONE, LAYOUT_A, LAYOUT_B } Layout;

// All run-time state shared between main.c and the helper functions.
typedef struct {
    Kobuki_Typedef kobuki;
    US_Typedef object;
    Layout layout;
    int16_t heading;
    uint8_t corridor;                // 1 = first corridor, 2 = second
    int16_t pathfind_recover_deg;    // turn to apply after the Reverse
    int16_t scan_prev_angle;         // last gyro reading, for the wrap-safe turn sum
    int32_t scan_ticks;              // total turn in 0.01 deg gyro ticks
    int16_t scan_deg;                // total turn in degrees
    US_Typedef deposit_rod;          // closest valid target seen during the Deposit scan
    int16_t deposit_rod_angle;       // gyro heading of that target, in 0.01 deg ticks
    uint16_t deposit_range_mm;       // live reading while adjusting distance; keeps the scan result above intact
    uint8_t deposit_settle_packets;  // packets left to hold still before the next distance check
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

// True once the AMU has turned a full circle anticlockwise.
bool Scan_Gave_Up(void);

// Start a fresh full-circle Deposit scan, forgetting the previous closest rod.
void Deposit_Scan_Start(int16_t angle_tick);

// Keep the shortest valid ultrasonic range and its gyro heading throughout the scan.
void Deposit_Scan_Sample(void);

// Signed shortest turn to the saved rod heading, in 0.01 degree ticks; positive is anticlockwise.
int16_t Deposit_Rod_Turn_Ticks(void);

// Update deposit_range_mm; false for zero or a maximum-range/no-echo reading.
bool Deposit_Read_Range(void);

// Storage-wall bumper in Drive. Latches the nudge away from the wall into pathfind_recover_deg.
bool Corridor_Take_Wall_Hit(uint8_t bumper);

// Ledge cliff in Drive. Latches the nudge away from the ledge into pathfind_recover_deg.
bool Corridor_Take_Ledge_Hit(const Kobuki_Typedef *kobuki);

#endif
