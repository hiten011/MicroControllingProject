#include "fsm_helpers.h"
#include "fsm_config.h"

FSM_Layout FSM_Layout_From_Range(uint16_t range_mm)
{
    if (range_mm > FSM_LAYOUT_SPLIT_MM && range_mm <= FSM_LAYOUT_A_MAX_MM) {
        return FSM_LAYOUT_A;
    }

    if (range_mm >= FSM_LAYOUT_B_MIN_MM && range_mm <= FSM_LAYOUT_SPLIT_MM) {
        return FSM_LAYOUT_B;
    }

    return FSM_LAYOUT_NONE;
}

int16_t FSM_Angle_Delta_Deg(int16_t from_angle_tick, int16_t to_angle_tick)
{
    int32_t delta = (int32_t)to_angle_tick - (int32_t)from_angle_tick;

    // Take the short way round when the reading wrapped.
    if (delta > FSM_ANGLE_TICKS_HALF_TURN) {
        delta -= FSM_ANGLE_TICKS_FULL_TURN;
    } else if (delta < -FSM_ANGLE_TICKS_HALF_TURN) {
        delta += FSM_ANGLE_TICKS_FULL_TURN;
    }

    // Round to the nearest degree so the error of each Step does not add up in one direction.
    int32_t half_deg = FSM_ANGLE_TICKS_PER_DEG / 2;
    return (int16_t)((delta >= 0 ? delta + half_deg : delta - half_deg) / FSM_ANGLE_TICKS_PER_DEG);
}
