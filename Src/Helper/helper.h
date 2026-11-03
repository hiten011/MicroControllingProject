#ifndef HELPER_H
#define HELPER_H

#include <stdbool.h>
#include <stdint.h>
#include "kobuki.h"
#include "ultrasonic.h"

// The Kobuki sends one feedback packet every 20 ms (50 Hz), which paces the main loop.
#define KOBUKI_PACKET_MS 20U

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

// Deposit scan and align: slow enough for the ultrasonic's 10 Hz output while turning anticlockwise.
#define DEPOSIT_SCAN_SPEED_MM_S 50

// Deposit align: stop facing the saved measurement within 1 degree (gyro ticks are 0.01 degree).
#define DEPOSIT_ALIGN_TOLERANCE_TICKS 100

// Deposit refine: sweep around the coarse heading, stopping at each step to take a median of 3 readings.
// Aligning to the refined heading then uses the tighter tolerance and slower speed.
#define DEPOSIT_REFINE_HALF_ANGLE_DEG  25
#define DEPOSIT_REFINE_STEP_DEG        2
#define DEPOSIT_REFINE_POINTS          ((2 * DEPOSIT_REFINE_HALF_ANGLE_DEG) / DEPOSIT_REFINE_STEP_DEG + 1)
#define DEPOSIT_REFINE_SPEED_MM_S      35
#define DEPOSIT_REFINE_TOLERANCE_TICKS 50
#define DEPOSIT_REFINE_RANGE_BAND_MM   10U
#define DEPOSIT_REFINE_SAMPLE_PACKETS  5U  // About 100 ms between readings at 50 Hz.

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

// Working data for the Deposit refine sweep: one median range and heading per step.
typedef struct {
    int16_t base_angle;                      // coarse heading the sweep is centred on
    int16_t angles[DEPOSIT_REFINE_POINTS];   // gyro heading each step was measured at
    uint16_t ranges[DEPOSIT_REFINE_POINTS];  // median range of each step
    uint16_t readings[3];                    // the three readings being taken for the current step
    uint8_t point;                           // current step
    uint8_t sample_count;                    // readings taken so far at this step
    uint8_t wait_packets;                    // packets left before the next reading
} DepositRefine;

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
    bool deposit_refined;            // true once the refine sweep has replaced the coarse heading
    DepositRefine deposit_refine;
    uint8_t win_sound_notes_left;    // notes of the win sound still to play; 0 = silent
    uint8_t win_sound_wait_packets;  // packets left before the next note
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

// Refine the coarse heading with a stationary sweep: turn to each step, settle, then take 3 readings.
void Deposit_Refine_Start(void);

// Signed shortest turn to the current sweep step, in 0.01 degree ticks; positive is anticlockwise.
int16_t Deposit_Refine_Turn_Ticks(void);

// Restart the settle wait and reading count for the step the AMU has just reached.
void Deposit_Refine_Settle(void);

// Take the next reading once settled; true when all 3 are in and their median is stored for this step.
bool Deposit_Refine_Sample(void);

// Move on to the next step; false once the sweep is complete.
bool Deposit_Refine_Next(void);

// Centre the widest contiguous group within 10 mm of the local minimum; false if no rod was seen.
bool Deposit_Refine_Finish(void);

// Update deposit_range_mm; false for zero or a maximum-range/no-echo reading.
bool Deposit_Read_Range(void);

// Start the win sound, a 3 s victory fanfare. Play it by calling Win_Sound_Tick once per Tick.
void Win_Sound_Start(void);

// Send the next note of the win sound when its turn comes; does nothing once it has finished.
void Win_Sound_Tick(void);

// Cut the win sound short. A note already sent still finishes on the Kobuki.
void Win_Sound_Stop(void);

// Storage-wall bumper in Drive. Latches the nudge away from the wall into pathfind_recover_deg.
bool Corridor_Take_Wall_Hit(uint8_t bumper);

// Ledge cliff in Drive. Latches the nudge away from the ledge into pathfind_recover_deg.
bool Corridor_Take_Ledge_Hit(const Kobuki_Typedef *kobuki);

#endif
