#include "helper.h"

// Shared run-time state.
RobotData robot;

bool Fault_B1_Wheeldrop(void)
{
    return (robot.kobuki.button & KOBUKI_BUTTON_B1_MASK) != 0 || robot.kobuki.wheeldrop != 0;
}

bool Fault_Any(void)
{
    return Fault_B1_Wheeldrop() || robot.kobuki.bumper != 0 ||
           Kobuki_IsCliffDetected(&robot.kobuki, KOBUKI_SIDE_ALL_MASK);
}

Layout Layout_From_Range(uint16_t range_mm)
{
    if (range_mm < LAYOUT_B_MAX_RANGE_MM) {
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

    // The gyro wraps at +-18000 ticks, so fold a jump of more than half a turn back.
    if (diff > 18000) {
        diff -= 36000;
    } else if (diff < -18000) {
        diff += 36000;
    }
    robot.scan_prev_angle = angle_tick;
    robot.scan_ticks += diff;

    // Round to the nearest degree (100 ticks per degree), away from zero.
    if (robot.scan_ticks >= 0) {
        robot.scan_deg = (int16_t)((robot.scan_ticks + 50) / 100);
    } else {
        robot.scan_deg = (int16_t)((robot.scan_ticks - 50) / 100);
    }
}

void Scan_Sample(void)
{
    Scan_Update_Turn(robot.kobuki.angle);
    Ultrasonic_Detect(&robot.object);
}

bool Scan_Gave_Up(void)
{
    return robot.scan_ticks >= (int32_t)SCAN_FULL_TURN_DEG * 100;
}

void Deposit_Scan_Start(int16_t angle_tick)
{
    Scan_Start(angle_tick);
    robot.deposit_rod.detected = 0;
    robot.deposit_rod.range_mm = ULTRASONIC_MAX_RANGE_MM;
    robot.deposit_rod_angle = angle_tick;
    robot.deposit_range_mm = ULTRASONIC_MAX_RANGE_MM;
    robot.deposit_settle_packets = 0;
}

void Deposit_Scan_Sample(void)
{
    Scan_Update_Turn(robot.kobuki.angle);
    uint16_t range_mm = Ultrasonic_Read();

    // Zero is invalid; the sensor's maximum range also represents no echo.
    if (range_mm > 0 && range_mm < robot.deposit_rod.range_mm) {
        robot.deposit_rod.detected = 1;
        robot.deposit_rod.range_mm = range_mm;
        robot.deposit_rod_angle = robot.kobuki.angle;
    }
}

int16_t Deposit_Rod_Turn_Ticks(void)
{
    int32_t turn = (int32_t)robot.deposit_rod_angle - robot.kobuki.angle;

    // The gyro wraps at +-18000 ticks, so take the shorter way round.
    if (turn > 18000) {
        turn -= 36000;
    } else if (turn < -18000) {
        turn += 36000;
    }
    return (int16_t)turn;
}

bool Deposit_Read_Range(void)
{
    robot.deposit_range_mm = Ultrasonic_Read();
    return robot.deposit_range_mm > 0 && robot.deposit_range_mm < ULTRASONIC_MAX_RANGE_MM;
}

bool Corridor_Take_Wall_Hit(uint8_t bumper)
{
    // The storage wall is on the left in Layout A and on the right in Layout B.
    uint8_t wall_mask = (robot.layout == LAYOUT_A) ? KOBUKI_SIDE_LEFT_MASK : KOBUKI_SIDE_RIGHT_MASK;

    if ((bumper & wall_mask) == 0) {
        return false;
    }
    robot.pathfind_recover_deg = Layout_Pick(PATHFIND_RECOVER_TURN_LAYOUT_A_DEG, PATHFIND_RECOVER_TURN_LAYOUT_B_DEG);
    return true;
}

bool Corridor_Take_Ledge_Hit(const Kobuki_Typedef *kobuki)
{
    // The ledge is on the right in Layout A and on the left in Layout B.
    uint8_t ledge_mask = (robot.layout == LAYOUT_A) ? KOBUKI_SIDE_RIGHT_MASK : KOBUKI_SIDE_LEFT_MASK;

    if (!Kobuki_IsCliffDetected(kobuki, ledge_mask)) {
        return false;
    }
    robot.pathfind_recover_deg =
        (int16_t)(-Layout_Pick(PATHFIND_RECOVER_TURN_LAYOUT_A_DEG, PATHFIND_RECOVER_TURN_LAYOUT_B_DEG));
    return true;
}
