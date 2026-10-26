#ifndef AMU_CONFIG_H
#define AMU_CONFIG_H

#include <stdint.h>
#include "kobuki.h"
#include "ultrasonic.h"

// Names used by the AMU FSM diagram (docs/amu_fsm.drawio).
// Values marked PLACEHOLDER are unverified: tune them on the hardware.

// Kobuki protocol codes. PLACEHOLDER until confirmed on the hardware.
#define BUTTON_0_START ((uint8_t)0x01U)
#define BUTTON_1_STOP  ((uint8_t)0x02U)
#define BUMPER_RIGHT   ((uint8_t)0x01U)
#define BUMPER_CENTRE  ((uint8_t)0x02U)
#define BUMPER_LEFT    ((uint8_t)0x04U)
#define CLIFF_RIGHT    ((uint8_t)0x01U)
#define CLIFF_CENTRE   ((uint8_t)0x02U)
#define CLIFF_LEFT     ((uint8_t)0x04U)

// Corridor driving: speed below SPEED_LIMIT, corrections in degrees.
#define PATH_SPEED ((int16_t)100)  // mm / s. PLACEHOLDER, try 80, 100, 120
#define WALL_DEG   ((int16_t)10)   // turn away from the storage wall. PLACEHOLDER, try 5 to 20
#define LEDGE_DEG  ((int16_t)10)   // turn away from the ledge. PLACEHOLDER, try 5 to 20

// Layout deduced from the range to the blocking bollard seen from the start circle.
// Near band [MIN, SPLIT] is Layout B, far band (SPLIT, MAX] is Layout A.
#define LAYOUT_MIN_RANGE_MM ((uint16_t)300U)  // mm. PLACEHOLDER
#define LAYOUT_RANGE_MM     ((uint16_t)450U)  // mm, split. PLACEHOLDER, about 475 minus the sensor offset
#define LAYOUT_MAX_RANGE_MM ((uint16_t)650U)  // mm. PLACEHOLDER

// Drive from the stop point into the storage unit.
#define ENTRY_MM ((int16_t)740)  // mm. PLACEHOLDER, geometric estimate

// SEARCH gives up after this many full 360 degree sweeps with nothing detected.
#define MAX_SEARCH_SWEEPS ((uint8_t)2U)

_Static_assert(PATH_SPEED > 0 && PATH_SPEED < SPEED_LIMIT, "PATH_SPEED must be below SPEED_LIMIT");
_Static_assert(LAYOUT_MIN_RANGE_MM < LAYOUT_RANGE_MM && LAYOUT_RANGE_MM < LAYOUT_MAX_RANGE_MM,
               "layout ranges must satisfy MIN < SPLIT < MAX");
_Static_assert(LAYOUT_MAX_RANGE_MM <= ULTRASONIC_DETECT_RANGE_MM, "layout bands must lie inside the detect range");

#endif
