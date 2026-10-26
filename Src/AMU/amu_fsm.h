#ifndef AMU_FSM_H
#define AMU_FSM_H

#include <stdint.h>
#include <stdbool.h>
#include "kobuki.h"
#include "amu_helpers.h"

typedef enum {
    AMU_IDLE,
    AMU_SEARCH,
    AMU_ALIGN,
    AMU_ORIENT,
    AMU_PATHFIND,
    AMU_STORAGE,
    AMU_DEPOSIT,
    AMU_STOP_FAULT
} AMU_State_Enum;

typedef enum { AMU_FIRST_CORRIDOR, AMU_SECOND_CORRIDOR } AMU_Corridor_Enum;

// PATHFIND sub-states, shared by both corridors
typedef enum {
    AMU_PF_DRIVE,
    AMU_PF_WALL_HIT,
    AMU_PF_LEDGE_HIT,
    AMU_PF_REALIGN,
    AMU_PF_FRONT_CLIFF_TURN
} AMU_Pathfind_Enum;

// STORAGE sub-states
typedef enum { AMU_ST_TURN, AMU_ST_DRIVE_IN } AMU_Storage_Enum;

typedef struct {
    AMU_State_Enum state;
    bool entry;  // first loop in the current state or sub-state: entry actions run, completion is not tested
    AMU_Layout_Enum layout;  // saved by SEARCH at each stop, read by the later states
    int16_t last_angle;      // gyro at the previous SEARCH loop
    int16_t swept_deg;       // angle swept in the current sweep, across the gyro wrap
    uint8_t sweep_count;     // full sweeps done with nothing detected
    AMU_Corridor_Enum corridor;
    AMU_Pathfind_Enum pathfind;
    AMU_Storage_Enum storage;
    int16_t heading;  // corridor heading latched on entry to each corridor (gyro, hundredths of a degree)
} AMU_Typedef;

void AMU_Init(AMU_Typedef *amu);

// One FSM step. Call once per received Kobuki packet, right after Kobuki_Read.
void AMU_Step(AMU_Typedef *amu, const Kobuki_Typedef *kobuki);

#endif
