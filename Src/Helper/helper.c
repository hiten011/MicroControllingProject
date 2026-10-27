#include "helper.h"

RobotData robot;

Layout Layout_From_Range(uint16_t range_mm)
{
    if (range_mm > LAYOUT_SPLIT_MM) {
        return LAYOUT_A;
    }
    return LAYOUT_B;
}
