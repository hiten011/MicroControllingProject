#include "helper.h"

RobotData robot;

Layout Layout_From_Range(uint16_t closest_mm)
{
    if (closest_mm < LAYOUT_B_MAX_MM) {
        return LAYOUT_B;
    }
    return LAYOUT_A;
}

void Scan_Start(int16_t angle_tick)
{
    robot.scan_prev_angle = angle_tick;
    robot.scan_ticks = 0;
    robot.scan_deg = 0;
    robot.first_seen_deg = NO_ANGLE;
    robot.last_seen_deg = 0;
    robot.closest_mm = ULTRASONIC_DETECT_RANGE_MM;
    robot.edge_mm = ULTRASONIC_DETECT_RANGE_MM;
    robot.edge_start_deg = NO_ANGLE;
    robot.edge_end_deg = -NO_ANGLE;
    robot.centre_deg = 0;
}

void Scan_Update_Turn(int16_t angle_tick)
{
    int32_t diff = (int32_t)angle_tick - robot.scan_prev_angle;

    if (diff > 18000) {
        diff -= 36000;
    } else if (diff < -18000) {
        diff += 36000;
    }
    robot.scan_prev_angle = angle_tick;
    robot.scan_ticks += diff;

    if (robot.scan_ticks >= 0) {
        robot.scan_deg = (int16_t)((robot.scan_ticks + 50) / 100);
    } else {
        robot.scan_deg = (int16_t)((robot.scan_ticks - 50) / 100);
    }
}
