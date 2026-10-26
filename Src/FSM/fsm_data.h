#ifndef FSM_DATA_H
#define FSM_DATA_H

#include <stdint.h>
#include "kobuki.h"
#include "ultrasonic.h"

typedef enum { FSM_LAYOUT_NONE, FSM_LAYOUT_A, FSM_LAYOUT_B } FSM_Layout;

// The values the conditions, effects, init() and exit() functions of the States read and write.
typedef struct {
    Kobuki_Typedef kobuki;  // Sensors, read at the top of every Tick with Kobuki_Read()
    US_Typedef object;      // Last Detect reading
    FSM_Layout layout;      // Learned by the Search
    int16_t swept_deg;      // Angle spun in the current Sweep, + = anticlockwise
    uint8_t sweep_count;    // Sweeps finished without a Detection
    int16_t last_angle;     // kobuki.angle (ticks) at the last Spin exit
} FSM_Data;

extern FSM_Data fsm_data;

#endif
