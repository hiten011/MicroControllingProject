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

bool Deposit_Marker_Seen(void)
{
    return robot.object.range_mm < DEPOSIT_DETECT_MM;
}

void Deposit_Keep_Closest(void)
{
    if (robot.object.range_mm < robot.closest) {
        robot.closest = robot.object.range_mm;
    }
}

void Deposit_Track_Sample(void)
{
    if (!Deposit_Marker_Seen()) {
        return;
    }
    robot.last_seen = robot.scan_deg;
    Deposit_Keep_Closest();
}

bool Deposit_Marker_Lost(void)
{
    return robot.scan_deg - robot.last_seen >= DEPOSIT_LOST_DEG;
}

void Deposit_Edge_Start(void)
{
    robot.edge_mm = (uint16_t)(robot.closest + DEPOSIT_EDGE_BAND_MM);
    robot.edge_start = DEPOSIT_EDGE_UNSET_DEG;
    robot.edge_end = -DEPOSIT_EDGE_UNSET_DEG;
}

void Deposit_Edge_Sample(void)
{
    Deposit_Keep_Closest();
    if (robot.object.range_mm > robot.edge_mm) {
        return;
    }
    if (robot.scan_deg < robot.edge_start) {
        robot.edge_start = robot.scan_deg;
    }
    if (robot.scan_deg > robot.edge_end) {
        robot.edge_end = robot.scan_deg;
    }
}

bool Deposit_Edge_Done(void)
{
    return robot.edge_start <= robot.edge_end && robot.scan_deg <= robot.edge_start - DEPOSIT_LOST_DEG;
}

bool Measure_Take_Reading(void)
{
    robot.samples_mm[robot.samples] = Ultrasonic_Read();
    robot.samples++;
    if (robot.samples < DEPOSIT_MEASURE_SAMPLES) {
        return false;
    }

    // Insertion sort, so the middle sample is the median.
    for (uint8_t i = 1U; i < DEPOSIT_MEASURE_SAMPLES; i++) {
        uint16_t value = robot.samples_mm[i];
        uint8_t j = i;

        while (j > 0U && robot.samples_mm[j - 1U] > value) {
            robot.samples_mm[j] = robot.samples_mm[j - 1U];
            j--;
        }
        robot.samples_mm[j] = value;
    }
    robot.reading = robot.samples_mm[DEPOSIT_MEASURE_SAMPLES / 2U];
    robot.drive_mm = (int16_t)((int32_t)robot.reading - DEPOSIT_GAP_READING_MM);
    return true;
}
