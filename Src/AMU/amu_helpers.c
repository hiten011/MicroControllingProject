#include <stdint.h>
#include "amu_config.h"
#include "amu_helpers.h"

#define ANGLE_TICK_OFFSET 18000  // [-18000, 18000] -> [0, 36000], as in Kobuki_Rotate_Angle_Setpoint
#define TICKS_PER_DEG     100

static int32_t Angle_Deg(int16_t angle_tick)
{
    return ((int32_t)angle_tick + ANGLE_TICK_OFFSET) / TICKS_PER_DEG;
}

int16_t AMU_Angle_Delta_Deg(int16_t from_angle_tick, int16_t to_angle_tick)
{
    int32_t delta = (Angle_Deg(to_angle_tick) - Angle_Deg(from_angle_tick)) % 360;

    // Fold into (-180, 180]
    if (delta > 180) {
        delta -= 360;
    } else if (delta <= -180) {
        delta += 360;
    }
    return (int16_t)delta;
}

AMU_Layout_Enum AMU_Layout_From_Range(uint16_t range_mm)
{
    if (range_mm < LAYOUT_MIN_RANGE_MM) {
        return AMU_LAYOUT_NONE;
    }
    if (range_mm <= LAYOUT_RANGE_MM) {
        return AMU_LAYOUT_B;
    }
    if (range_mm <= LAYOUT_MAX_RANGE_MM) {
        return AMU_LAYOUT_A;
    }
    return AMU_LAYOUT_NONE;
}
