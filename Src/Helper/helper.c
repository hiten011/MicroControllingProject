#include "helper.h"

RobotData robot;

bool Fault_B1_Wheeldrop(void)
{
    return (robot.kobuki.button & BUTTON_B1) != 0 || robot.kobuki.wheeldrop != 0;
}

bool Fault_Any(void)
{
    return Fault_B1_Wheeldrop() || robot.kobuki.bumper != 0 || Kobuki_IsCliffDetected(&robot.kobuki, SIDE_ANY);
}

Layout Layout_From_Range(uint16_t range_mm)
{
    if (range_mm < LAYOUT_B_MAX_MM) {
        return LAYOUT_B;
    }
    return LAYOUT_A;
}

int16_t Layout_Pick(int16_t a_deg, int16_t b_deg)
{
    return (robot.layout == LAYOUT_A) ? a_deg : b_deg;
}

void Scan_Start(int16_t angle_tick)
{
    robot.scan_prev_angle = angle_tick;
    robot.scan_ticks = 0;
    robot.scan_deg = 0;
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

void Delay_ms(uint32_t ms)
{
    volatile uint32_t count;
    while (ms > 0) {
        count = 0;
        while (count < 10000) {
            count++;
        }
        ms--;
    }
}


void Scan_Sample(void)
{
    Scan_Update_Turn(robot.kobuki.angle);
    Ultrasonic_Detect(&robot.object);
}

bool Scan_Gave_Up(void)
{
    return robot.scan_deg >= SCAN_FULL_DEG;
}

bool Corridor_Take_Wall_Hit(uint8_t bumper)
{
    uint8_t wall_mask = (robot.layout == LAYOUT_A) ? SIDE_LEFT : SIDE_RIGHT;

    if ((bumper & wall_mask) == 0) {
        return false;
    }
    robot.pathfind_recover_deg = Layout_Pick(PATHFIND_ROTATE_A_DEG, PATHFIND_ROTATE_B_DEG);
    return true;
}

bool Corridor_Take_Ledge_Hit(const Kobuki_Typedef *kobuki)
{
    uint8_t ledge_mask = (robot.layout == LAYOUT_A) ? SIDE_RIGHT : SIDE_LEFT;

    if (!Kobuki_IsCliffDetected(kobuki, ledge_mask)) {
        return false;
    }
    robot.pathfind_recover_deg = (int16_t)(-Layout_Pick(PATHFIND_ROTATE_A_DEG, PATHFIND_ROTATE_B_DEG));
    return true;
}
