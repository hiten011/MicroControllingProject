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
#define SCAN_SPEED_MM_S 150

// The first reading on a bollard is under this for Layout B (the nearer bollard), otherwise Layout A.
#define LAYOUT_B_MAX_MM 575U

// Orient: Layout A turns CW to the bottom corridor; Layout B turns ACW to the left corridor.
#define ORIENT_A_DEG (-75)
#define ORIENT_B_DEG (110)

#define PATHFIND_DRIVE_SPEED  150
#define PATHFIND_CORNER_A_DEG 90
#define PATHFIND_CORNER_B_DEG (-90)
#define PATHFIND_REVERSE_MM   50
#define PATHFIND_ROTATE_A_DEG (-5)
#define PATHFIND_ROTATE_B_DEG 5

#define STORAGE_TURN_A_DEG  135
#define STORAGE_TURN_B_DEG  (-135)
#define STORAGE_DRIVE_IN_MM 900

// Deposit: a reading under DEPOSIT_DETECT_MM is the Deposit marker, not the 750 mm Detection range.
#define DEPOSIT_DETECT_MM       500U
#define DEPOSIT_SCAN_SPEED_MM_S 50
#define DEPOSIT_LOST_DEG        10
#define DEPOSIT_EDGE_BAND_MM    60U
#define DEPOSIT_EDGE_UNSET_DEG  10000

// The ultrasonic reads about 300 mm for anything nearer, so a reading under DEPOSIT_BLIND_MM is not a distance.
#define DEPOSIT_MEASURE_SAMPLES 5U
#define DEPOSIT_BLIND_MM        320U
#define DEPOSIT_BACKOFF_MM      150
#define DEPOSIT_BACKOFF_MAX     5U

// The reading when the AMU stands at the Deposit gap (100 mm from the Deposit marker). Measure it on the AMU.
// At the gap the sonar can be in the Blind zone and read about 300 mm: measure further back, then subtract the extra.
#define DEPOSIT_GAP_READING_MM 277

typedef enum { LAYOUT_NONE, LAYOUT_A, LAYOUT_B } Layout;

typedef struct {
    Kobuki_Typedef kobuki;
    US_Typedef object;
    Layout layout;
    int16_t heading;
    uint8_t corridor;
    int16_t pathfind_recover_deg;
    int16_t scan_prev_angle;
    int32_t scan_ticks;
    int16_t scan_deg;
    uint8_t backoffs;
    int16_t last_seen;
    uint16_t closest;
    uint16_t edge_mm;
    int16_t edge_start;
    int16_t edge_end;
    int16_t centre_deg;
    uint8_t samples;
    uint16_t samples_mm[DEPOSIT_MEASURE_SAMPLES];
    uint16_t reading;
    int16_t drive_mm;
} RobotData;

extern RobotData robot;

// B1 pressed or a wheel dropped.
bool Fault_B1_Wheeldrop(void);

// B1, wheeldrop, any bumper or any cliff.
bool Fault_Any(void);

// Layout B when range_mm is under LAYOUT_B_MAX_MM, otherwise Layout A.
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

// True when the last reading is under DEPOSIT_DETECT_MM.
bool Deposit_Marker_Seen(void);

// Keep the smaller of closest and the last reading.
void Deposit_Keep_Closest(void);

// While the Deposit marker is seen, latch last_seen and keep the closest reading.
void Deposit_Track_Sample(void);

// True once the AMU has turned DEPOSIT_LOST_DEG past last_seen.
bool Deposit_Marker_Lost(void);

// Set the Edge band above the closest reading and clear both edges. Call when Deposit edge scan starts.
void Deposit_Edge_Start(void);

// Keep the closest reading, and widen the edges when the last reading is inside the Edge band.
void Deposit_Edge_Sample(void);

// True once the AMU has turned DEPOSIT_LOST_DEG past the edge it turns towards, with both edges found.
bool Deposit_Edge_Done(void);

// Take one ultrasonic reading. On the fifth, latch the median into reading and the signed distance to
// the Gap reading into drive_mm, and return true.
bool Measure_Take_Reading(void);

#endif
