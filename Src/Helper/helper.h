#ifndef HELPER_H
#define HELPER_H

#include <stdint.h>
#include "fsm_states.h"

// The Layout that a bollard at range_mm means, or FSM_LAYOUT_NONE if the range fits neither.
FSM_Layout Layout_From_Range(uint16_t range_mm);

// The shortest signed angle in whole degrees from one kobuki.angle reading to another.
// + = anticlockwise. The Kobuki reading wraps at +/-180 degrees.
int16_t Angle_Delta_Deg(int16_t from_angle_tick, int16_t to_angle_tick);

#endif
