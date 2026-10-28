#ifndef HELPER_H
#define HELPER_H

#include <stdint.h>
#include "kobuki.h"
#include "ultrasonic.h"

#define BUTTON_B0 0x01U
#define BUTTON_B1 0x02U

#define SIDE_RIGHT  0x01U
#define SIDE_CENTRE 0x02U
#define SIDE_LEFT   0x04U

#define SPIN_STEP_DEG 15

// Inclusive Layout range bands (mm). Gap or overlap classifies as LAYOUT_NONE.
#define LAYOUT_B_MIN_MM 500U
#define LAYOUT_B_MAX_MM 750U
#define LAYOUT_A_MIN_MM 800U
#define LAYOUT_A_MAX_MM 1000U

#define ALIGNMENT_OFFSET_DEG 13

// Orient: Layout A turns CW to the bottom corridor; Layout B turns ACW to the left corridor.
#define ORIENT_A_DEG (90)
#define ORIENT_B_DEG (-90)

#define PATHFIND_DRIVE_SPEED 100
// Figure 2 centreline legs: A bottom 1200 then right 1400; B left 1400 then top 1200.
#define PATHFIND_CORRIDOR_1_A_MM 1200
#define PATHFIND_CORRIDOR_2_A_MM 1400
#define PATHFIND_CORRIDOR_1_B_MM 1400
#define PATHFIND_CORRIDOR_2_B_MM 1200
#define PATHFIND_CORNER_A_DEG    90
#define PATHFIND_CORNER_B_DEG    (-90)
#define PATHFIND_REVERSE_MM      50
#define PATHFIND_ROTATE_A_DEG    (-10)
#define PATHFIND_ROTATE_B_DEG    10

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
    uint16_t corridor_start_distance;
    int16_t pathfind_recover_deg;
} RobotData;

extern RobotData robot;

// Layout A or B when range_mm is in that band alone; otherwise LAYOUT_NONE.
Layout Layout_From_Range(uint16_t range_mm);

#endif
