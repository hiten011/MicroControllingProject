#ifndef AMU_HELPERS_H
#define AMU_HELPERS_H

#include <stdint.h>

typedef enum { AMU_LAYOUT_NONE, AMU_LAYOUT_A, AMU_LAYOUT_B } AMU_Layout_Enum;

// Shortest signed rotation in whole degrees (+ve is ACW) from one gyro reading to another.
// Readings are in hundredths of a degree and wrap at +/-180 degrees. Each reading is first reduced to
// whole degrees the same way the Kobuki angle setpoint does, so the differences of successive readings
// add up exactly (no drift) and a rotation to the result ends on the whole degree of the target reading.
int16_t AMU_Angle_Delta_Deg(int16_t from_angle_tick, int16_t to_angle_tick);

// Classify a range to the blocking bollard: near band is Layout B, far band is Layout A, otherwise none.
AMU_Layout_Enum AMU_Layout_From_Range(uint16_t range_mm);

#endif
