#include "helper.h"

RobotData robot;

Layout Layout_From_Range(uint16_t range_mm)
{
    int in_a = (range_mm >= LAYOUT_A_MIN_MM && range_mm <= LAYOUT_A_MAX_MM);
    int in_b = (range_mm >= LAYOUT_B_MIN_MM && range_mm <= LAYOUT_B_MAX_MM);
    if (in_a && !in_b) {
        return LAYOUT_A;
    }
    if (in_b && !in_a) {
        return LAYOUT_B;
    }
    return LAYOUT_NONE;
}
