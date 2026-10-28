#include "helper.h"

RobotData robot;

Layout Layout_From_Range(uint16_t closest_mm)
{
    if (closest_mm < LAYOUT_B_MAX_MM) {
        return LAYOUT_B;
    }
    return LAYOUT_A;
}

void Robot_Start(int16_t angle_tick)
{
    robot.layout = LAYOUT_NONE;
    robot.object.detected = 0;
    robot.heading = 0;
    robot.corridor = 0;
    robot.corridor_start_distance = 0;
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

void Corridor_Begin(uint8_t index)
{
    robot.corridor = index;
    robot.corridor_start_distance = robot.kobuki.distance;
}

int16_t Corridor_Length_Mm(void)
{
    if (robot.layout == LAYOUT_A) {
        return (robot.corridor == 1) ? PATHFIND_CORRIDOR_1_A_MM
                                      : PATHFIND_CORRIDOR_2_A_MM;
    }
    return (robot.corridor == 1) ? PATHFIND_CORRIDOR_1_B_MM
                                 : PATHFIND_CORRIDOR_2_B_MM;
}

bool Corridor_Wall_Hit(uint8_t bumper)
{
    uint8_t wall_mask = (robot.layout == LAYOUT_A) ? SIDE_LEFT : SIDE_RIGHT;

    if ((bumper & wall_mask) == 0) {
        return false;
    }
    robot.pathfind_recover_deg = (robot.layout == LAYOUT_A) ? PATHFIND_ROTATE_A_DEG
                                                            : PATHFIND_ROTATE_B_DEG;
    return true;
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
