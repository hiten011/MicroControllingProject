// Exercise the real firmware loop with a moving rod range and delayed 10 Hz sonar.
// Build/run with: python3 sim/check_deposit_distance.py
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main firmware_main
#include "../Src/main.c"
#undef main

typedef enum {
    INJECT_NONE,
    INJECT_B1,
    INJECT_DROP,
    INJECT_BUMPER,
    INJECT_CLIFF,
    INJECT_ZERO,
    INJECT_NO_ECHO,
    INJECT_OVERSHOOT,
    INJECT_LOST_ROD
} Injection;

static jmp_buf finished;
static double gap, gyro, left_speed, right_speed, left_target, right_target;
static double largest_gap;
static double rod_heading = 9000.0;
static uint16_t sonar, pending_sonar;
static unsigned packets, reverse_commands, forward_commands, injected_at, finished_at;
static Kobuki_LED_Enum led;
static Injection injection;
static State injection_state;
static bool saw_reverse, saw_approach, add_noise, disturbed, lost_rod;

static int16_t gyro_tick(double angle)
{
    while (angle > 18000.0) {
        angle -= 36000.0;
    }
    while (angle < -18000.0) {
        angle += 36000.0;
    }
    return (int16_t)lround(angle);
}

static double accelerate(double current, double target)
{
    double step = 500.0 * 0.02;
    if (target > current) {
        return fmin(current + step, target);
    }
    return fmax(current - step, target);
}

void UART_Init(void)
{
}
void ADC_Init(void)
{
}
void Ultrasonic_Init(void)
{
}

void Kobuki_Read(Kobuki_Typedef *kobuki)
{
    assert(++packets < 8000); // A stalled controller must fail the test.
    left_speed = accelerate(left_speed, left_target);
    right_speed = accelerate(right_speed, right_target);
    gyro += ((right_speed - left_speed) / 230.0) * 0.02 * (18000.0 / 3.141592653589793);
    gap -= ((left_speed + right_speed) / 2.0) * 0.02;
    if (gap > largest_gap) {
        largest_gap = gap;
    }
    if (injection == INJECT_OVERSHOOT && state == STATE_DEPOSIT_DISTANCE && saw_approach && !disturbed) {
        // Force a too-short settled reading after forward travel; it must back off and approach again.
        gap = DEPOSIT_TARGET_RANGE_MM - DEPOSIT_RANGE_TOLERANCE_MM - 20;
        disturbed = true;
    }
    if (injection == INJECT_LOST_ROD && state == STATE_DEPOSIT_REFINE_TURN) {
        lost_rod = true;
    }

    // Publish each measurement 100 ms later, and hold its value for 100 ms.
    if (packets % 5 == 0) {
        sonar = pending_sonar;
        int16_t offset = gyro_tick(gyro - rod_heading);
        int noise = add_noise ? (int)((packets / 5) % 3) * 3 - 3 : 0;
        pending_sonar = (!lost_rod && offset > -1200 && offset < 1200) ? (uint16_t)lround(gap + noise) : 5000;
    }

    *kobuki = (Kobuki_Typedef){.angle = gyro_tick(gyro)};
    if (packets == 1) {
        // Start in Deposit as if Drive-in had just finished, taking the heading from this first packet.
        change_to(STATE_DEPOSIT);
    }
    saw_reverse |= state == STATE_DEPOSIT_REVERSE;
    saw_approach |= state == STATE_DEPOSIT_APPROACH;
    if (injection > INJECT_NONE && injection <= INJECT_NO_ECHO && state == injection_state && !injected_at) {
        // Exercise the stop while moving, rather than only on state entry.
        if (state == STATE_DEPOSIT_DISTANCE || state == STATE_DEPOSIT_REFINE_SAMPLE ||
            (left_target != 0 && right_target != 0)) {
            injected_at = packets;
        }
    }
    if (injected_at) {
        if (packets == injected_at) {
            if (injection == INJECT_B1) {
                kobuki->button = KOBUKI_BUTTON_B1_MASK;
            }
            if (injection == INJECT_DROP) {
                kobuki->wheeldrop = 1;
            }
            if (injection == INJECT_BUMPER) {
                kobuki->bumper = KOBUKI_SIDE_CENTRE_MASK;
            }
            if (injection == INJECT_CLIFF) {
                kobuki->cliff = KOBUKI_SIDE_CENTRE_MASK;
            }
        }
        if (injection == INJECT_ZERO) {
            sonar = 0;
        }
        if (injection == INJECT_NO_ECHO) {
            sonar = 5000;
        }
        if (packets > injected_at) {
            assert(left_target == 0 && right_target == 0);
        }
    }

    // Give Idle enough packets to expose an unintended automatic restart.
    if (packets > 5 && state == STATE_IDLE && !finished_at) {
        finished_at = packets;
    }
    if (finished_at && packets - finished_at >= 20) {
        assert(left_target == 0 && right_target == 0);
        longjmp(finished, 1);
    }
}

void Kobuki_Drive(int16_t speed)
{
    assert(speed == STOP || speed == -DEPOSIT_REVERSE_SPEED_MM_S || speed == DEPOSIT_APPROACH_SPEED_MM_S);
    if (speed < 0) {
        reverse_commands++;
    }
    if (speed > 0) {
        forward_commands++;
    }
    left_target = right_target = speed;
}

void Kobuki_Rotate(int16_t speed)
{
    assert(speed == DEPOSIT_SCAN_SPEED_MM_S || speed == -DEPOSIT_SCAN_SPEED_MM_S ||
           speed == DEPOSIT_REFINE_SPEED_MM_S || speed == -DEPOSIT_REFINE_SPEED_MM_S);
    left_target = -speed;
    right_target = speed;
}

void Kobuki_Update_LEDs(Kobuki_LED_Enum first, Kobuki_LED_Enum second)
{
    (void)second;
    led = first;
}

uint16_t Ultrasonic_Read(void)
{
    return sonar;
}

void Ultrasonic_Detect(US_Typedef *object)
{
    object->range_mm = sonar;
    object->detected = sonar < ULTRASONIC_DETECT_RANGE_MM;
}

bool Kobuki_IsCliffDetected(const Kobuki_Typedef *kobuki, uint8_t mask)
{
    return (kobuki->cliff & mask) != 0;
}

void Kobuki_Rotate_Angle_Setpoint(int16_t current, int16_t target)
{
    (void)current;
    (void)target;
    assert(!"Deposit must use the live saved-heading check");
}
void Kobuki_Rotate_Angle(int16_t angle)
{
    (void)angle;
    assert(0);
}
void Kobuki_Drive_Distance_Setpoint(uint16_t current, int16_t target)
{
    (void)current;
    (void)target;
    assert(!"Deposit must adjust using live sonar, not the scan's old range");
}
void Kobuki_Drive_Distance(uint16_t distance)
{
    (void)distance;
    assert(0);
}

static void run(double initial_gap, Injection event, State event_state, bool noise)
{
    memset(&robot, 0, sizeof robot);
    state = STATE_IDLE;
    gap = largest_gap = initial_gap;
    gyro = left_speed = right_speed = left_target = right_target = 0;
    sonar = pending_sonar = 5000;
    packets = reverse_commands = forward_commands = injected_at = finished_at = 0;
    led = OFF;
    injection = event;
    injection_state = event_state;
    saw_reverse = saw_approach = false;
    disturbed = false;
    lost_rod = false;
    add_noise = noise;
    if (setjmp(finished) == 0) {
        firmware_main();
        assert(!"firmware loop returned");
    }
    assert(state == STATE_IDLE);
}

static void check_refine_samples(void)
{
    memset(&robot, 0, sizeof robot);
    robot.deposit_rod_angle = 17900;
    Deposit_Refine_Start();
    Deposit_Refine_Settle();

    // A single bad echo is rejected; each of the three readings is spaced by a sonar cycle.
    sonar = 450;
    for (unsigned i = 0; i < DEPOSIT_SETTLE_PACKETS; i++) {
        assert(!Deposit_Refine_Sample());
    }
    assert(robot.deposit_refine.sample_count == 1);
    sonar = 0;
    for (unsigned i = 0; i < DEPOSIT_REFINE_SAMPLE_PACKETS; i++) {
        assert(!Deposit_Refine_Sample());
    }
    assert(robot.deposit_refine.sample_count == 2);
    sonar = 452;
    for (unsigned i = 1; i < DEPOSIT_REFINE_SAMPLE_PACKETS; i++) {
        assert(!Deposit_Refine_Sample());
    }
    assert(Deposit_Refine_Sample());
    assert(robot.deposit_refine.ranges[0] == 452);

    Deposit_Refine_Start();
    assert(!Deposit_Refine_Finish());
    assert(!robot.deposit_refined);
    for (unsigned i = 0; i < DEPOSIT_REFINE_POINTS; i++) {
        int32_t offset = (-DEPOSIT_REFINE_HALF_ANGLE_DEG + (int)i * DEPOSIT_REFINE_STEP_DEG) * 100;
        robot.deposit_refine.angles[i] = gyro_tick(17900 + offset);
    }
    // The close echo crosses the gyro wrap. A wider, farther echo must not win.
    for (unsigned i = 10; i <= 16; i++) {
        robot.deposit_refine.ranges[i] = 410;
    }
    for (unsigned i = 18; i < DEPOSIT_REFINE_POINTS; i++) {
        robot.deposit_refine.ranges[i] = 600;
    }
    assert(Deposit_Refine_Finish());
    assert(robot.deposit_refined && robot.deposit_rod.range_mm == 410);
    assert(abs(gyro_tick(robot.deposit_rod_angle - 18000)) <= 1);
    puts("PASS: stationary median, empty scan, nearer echo selection and cluster centre across gyro wrap");
}

int main(void)
{
    check_refine_samples();
    // Start gaps are offsets from the target, so the check follows DEPOSIT_TARGET_RANGE_MM if it is retuned.
    const double offsets[] = {-120, -70, -11, -10, 0, 10, 11, 100, 380};
    const double target = DEPOSIT_TARGET_RANGE_MM;
    for (unsigned i = 0; i < sizeof offsets / sizeof offsets[0]; i++) {
        const double start = target + offsets[i];
        for (unsigned noise = 0; noise < 2; noise++) {
            run(start, INJECT_NONE, STATE_IDLE, noise != 0);
            assert(led == GREEN && robot.deposit_rod.detected && robot.deposit_refined);
            assert(abs(gyro_tick(robot.deposit_rod_angle - rod_heading)) <= 200);
            assert(abs(gyro_tick(gyro - rod_heading)) <= 200);
            // The real gap also includes the injected ±3 mm measurement error and ADC rounding.
            double measurement_error = noise ? 3.5 : 0.5;
            assert(gap >= DEPOSIT_TARGET_RANGE_MM - DEPOSIT_RANGE_TOLERANCE_MM - measurement_error);
            assert(gap <= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM + measurement_error);
            assert(robot.deposit_range_mm >= DEPOSIT_TARGET_RANGE_MM - DEPOSIT_RANGE_TOLERANCE_MM);
            assert(robot.deposit_range_mm <= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM);
            if (offsets[i] <= -70) {
                assert(saw_reverse && saw_approach && reverse_commands && forward_commands);
                assert(largest_gap >= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_REVERSE_MARGIN_MM);
            }
            if (!noise && offsets[i] >= -(double)DEPOSIT_RANGE_TOLERANCE_MM &&
                offsets[i] <= (double)DEPOSIT_RANGE_TOLERANCE_MM) {
                assert(!reverse_commands && !forward_commands);
            }
            if (offsets[i] >= 100) {
                assert(!reverse_commands && forward_commands);
            }
            printf("PASS initial %.0f mm%s: settled at %.1f mm (live %u mm)\n", start, noise ? " with noise" : "",
                   gap, robot.deposit_range_mm);
        }
    }

    const double headings[] = {0, 17900, -17900, 4500, 9000};
    for (unsigned i = 0; i < sizeof headings / sizeof headings[0]; i++) {
        rod_heading = headings[i];
        run(DEPOSIT_TARGET_RANGE_MM, INJECT_NONE, STATE_IDLE, true);
        assert(led == GREEN && robot.deposit_refined);
        int16_t coarse_error = gyro_tick(robot.deposit_refine.base_angle - rod_heading);
        int16_t refined_error = gyro_tick(robot.deposit_rod_angle - rod_heading);
        assert(abs(refined_error) <= 200);
        assert(abs(gyro_tick(gyro - rod_heading)) <= 200);
        printf("PASS heading %.2f deg: coarse error %.2f deg, refined error %.2f deg\n",
               rod_heading / 100.0, coarse_error / 100.0, refined_error / 100.0);
    }
    rod_heading = 9000;

    run(DEPOSIT_TARGET_RANGE_MM, INJECT_LOST_ROD, STATE_IDLE, false);
    assert(lost_rod && !robot.deposit_refined && led == RED);
    assert(!forward_commands && !reverse_commands);
    puts("PASS: losing the rod during refinement stops before linear movement");

    run(target + 380, INJECT_OVERSHOOT, STATE_DEPOSIT_DISTANCE, false);
    assert(disturbed && saw_reverse && reverse_commands && forward_commands && led == GREEN);
    assert(robot.deposit_range_mm >= DEPOSIT_TARGET_RANGE_MM - DEPOSIT_RANGE_TOLERANCE_MM);
    assert(robot.deposit_range_mm <= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM);
    puts("PASS: a too-short settled reading repeats reverse-then-approach correction");

    const State phases[] = {STATE_DEPOSIT_REVERSE, STATE_DEPOSIT_APPROACH, STATE_DEPOSIT_DISTANCE};
    for (unsigned phase = 0; phase < sizeof phases / sizeof phases[0]; phase++) {
        for (Injection event = INJECT_B1; event <= INJECT_NO_ECHO; event++) {
            run(phases[phase] == STATE_DEPOSIT_APPROACH ? target + 380 : target - 70, event, phases[phase], false);
            assert(injected_at);
            assert(led != GREEN);
            if (event == INJECT_ZERO || event == INJECT_NO_ECHO) {
                assert(led == RED);
            }
            printf("PASS immediate stop: phase %u, event %u\n", phase, (unsigned)event);
        }
    }
    const State refine_phases[] = {STATE_DEPOSIT_REFINE_TURN, STATE_DEPOSIT_REFINE_SAMPLE};
    for (unsigned phase = 0; phase < sizeof refine_phases / sizeof refine_phases[0]; phase++) {
        for (Injection event = INJECT_B1; event <= INJECT_DROP; event++) {
            run(DEPOSIT_TARGET_RANGE_MM, event, refine_phases[phase], false);
            assert(injected_at && led != GREEN && !robot.deposit_refined);
            assert(!forward_commands && !reverse_commands);
            printf("PASS refinement stop: phase %u, event %u\n", phase, (unsigned)event);
        }
    }
    puts("PASS: local refinement, calibrated distance, reverse then approach, latency/noise and stops");
    return 0;
}
