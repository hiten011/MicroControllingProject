#include "helper.h"

// Shared run-time state.
RobotData robot;

// Fold an angle difference back into +-18000 ticks (the gyro wraps there), so the short way round is chosen.
static int16_t angle_wrap(int32_t angle)
{
    if (angle > 18000) {
        angle -= 36000;
    } else if (angle < -18000) {
        angle += 36000;
    }
    return (int16_t)angle;
}

// Signed shortest turn from current to target, in 0.01 degree ticks; positive is anticlockwise.
static int16_t angle_turn(int16_t target, int16_t current)
{
    return angle_wrap((int32_t)target - current);
}

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
    robot.deposit_refined = false;
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
    return angle_turn(robot.deposit_rod_angle, robot.kobuki.angle);
}

void Deposit_Refine_Start(void)
{
    robot.deposit_refine = (DepositRefine){.base_angle = robot.deposit_rod_angle};
    for (uint8_t i = 0; i < DEPOSIT_REFINE_POINTS; i++) {
        robot.deposit_refine.ranges[i] = ULTRASONIC_MAX_RANGE_MM;
    }
    robot.deposit_refined = false;
}

int16_t Deposit_Refine_Turn_Ticks(void)
{
    DepositRefine *refine = &robot.deposit_refine;
    int32_t offset = (-DEPOSIT_REFINE_HALF_ANGLE_DEG + refine->point * DEPOSIT_REFINE_STEP_DEG) * 100;
    return angle_turn(angle_wrap((int32_t)refine->base_angle + offset), robot.kobuki.angle);
}

void Deposit_Refine_Settle(void)
{
    robot.deposit_refine.sample_count = 0;
    robot.deposit_refine.wait_packets = DEPOSIT_SETTLE_PACKETS;
}

bool Deposit_Refine_Sample(void)
{
    DepositRefine *refine = &robot.deposit_refine;
    if (refine->wait_packets > 0) {
        refine->wait_packets--;
        if (refine->wait_packets > 0) {
            return false;
        }
    }

    // Zero is invalid, so treat it like no echo.
    uint16_t range = Ultrasonic_Read();
    refine->readings[refine->sample_count++] = (range > 0) ? range : ULTRASONIC_MAX_RANGE_MM;
    if (refine->sample_count < 3) {
        refine->wait_packets = DEPOSIT_REFINE_SAMPLE_PACKETS;
        return false;
    }

    // Sort just these three readings; one bad/noisy echo cannot choose the heading by itself.
    for (uint8_t i = 0; i < 2; i++) {
        for (uint8_t j = i + 1; j < 3; j++) {
            if (refine->readings[j] < refine->readings[i]) {
                uint16_t temp = refine->readings[i];
                refine->readings[i] = refine->readings[j];
                refine->readings[j] = temp;
            }
        }
    }
    refine->ranges[refine->point] = refine->readings[1];
    refine->angles[refine->point] = robot.kobuki.angle;
    return true;
}

bool Deposit_Refine_Next(void)
{
    robot.deposit_refine.point++;
    return robot.deposit_refine.point < DEPOSIT_REFINE_POINTS;
}

bool Deposit_Refine_Finish(void)
{
    DepositRefine *refine = &robot.deposit_refine;
    uint16_t minimum = ULTRASONIC_MAX_RANGE_MM;
    for (uint8_t i = 0; i < DEPOSIT_REFINE_POINTS; i++) {
        if (refine->ranges[i] < minimum) {
            minimum = refine->ranges[i];
        }
    }
    if (minimum == ULTRASONIC_MAX_RANGE_MM) {
        return false;
    }

    // Find the widest run of steps within the band of the minimum, preferring the one nearest the coarse heading.
    uint8_t best_count = 0;
    int16_t best_angle = refine->base_angle;
    int32_t best_offset = 36000;
    for (uint8_t i = 0; i < DEPOSIT_REFINE_POINTS;) {
        if (refine->ranges[i] >= ULTRASONIC_MAX_RANGE_MM ||
            refine->ranges[i] > minimum + DEPOSIT_REFINE_RANGE_BAND_MM) {
            i++;
            continue;
        }
        uint8_t first = i;
        do {
            i++;
        } while (i < DEPOSIT_REFINE_POINTS && refine->ranges[i] < ULTRASONIC_MAX_RANGE_MM &&
                 refine->ranges[i] <= minimum + DEPOSIT_REFINE_RANGE_BAND_MM);

        uint8_t count = i - first;
        int16_t span = angle_turn(refine->angles[i - 1], refine->angles[first]);
        int16_t centre = angle_wrap((int32_t)refine->angles[first] + span / 2);
        int32_t offset = angle_turn(centre, refine->base_angle);
        if (offset < 0) {
            offset = -offset;
        }
        if (count > best_count || (count == best_count && offset < best_offset)) {
            best_count = count;
            best_angle = centre;
            best_offset = offset;
        }
    }
    robot.deposit_rod.range_mm = minimum;
    robot.deposit_rod_angle = best_angle;
    robot.deposit_refined = true;
    return true;
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
