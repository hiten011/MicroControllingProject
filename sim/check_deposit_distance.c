// Exercise the real firmware loop with a moving rod range and delayed 10 Hz sonar.
// Build/run with: python3 sim/check_deposit_distance.py
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
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
    INJECT_OVERSHOOT
} Injection;

static jmp_buf finished;
static double gap, gyro, left_speed, right_speed, left_target, right_target;
static double largest_gap;
static uint16_t sonar, pending_sonar;
static unsigned packets, reverse_commands, forward_commands, injected_at, finished_at;
static Kobuki_LED_Enum led;
static Injection injection;
static State injection_state;
static bool saw_reverse, saw_approach, add_noise, disturbed;

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
    if (injection == INJECT_OVERSHOOT && state == ST_DEPOSIT_DISTANCE && saw_approach && !disturbed) {
        // Force a too-short settled reading after forward travel; it must back off and approach again.
        gap = 390;
        disturbed = true;
    }

    // Publish each measurement 100 ms later, and hold its value for 100 ms.
    if (packets % 5 == 0) {
        sonar = pending_sonar;
        int16_t offset = gyro_tick(gyro - 9000.0);
        int noise = add_noise ? (int)((packets / 5) % 3) * 3 - 3 : 0;
        pending_sonar = (offset > -1200 && offset < 1200) ? (uint16_t)lround(gap + noise) : 5000;
    }

    *kobuki = (Kobuki_Typedef){.angle = gyro_tick(gyro)};
    saw_reverse |= state == ST_DEPOSIT_REVERSE;
    saw_approach |= state == ST_DEPOSIT_APPROACH;
    if (injection > INJECT_NONE && injection <= INJECT_NO_ECHO && state == injection_state && !injected_at) {
        // Exercise the stop while moving, rather than only on state entry.
        if (state == ST_DEPOSIT_DISTANCE || (left_target != 0 && right_target != 0)) {
            injected_at = packets;
        }
    }
    if (injected_at) {
        if (packets == injected_at) {
            if (injection == INJECT_B1) {
                kobuki->button = BUTTON_B1;
            }
            if (injection == INJECT_DROP) {
                kobuki->wheeldrop = 1;
            }
            if (injection == INJECT_BUMPER) {
                kobuki->bumper = SIDE_CENTRE;
            }
            if (injection == INJECT_CLIFF) {
                kobuki->cliff = SIDE_CENTRE;
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
    if (packets > 5 && state == ST_IDLE && !finished_at) {
        finished_at = packets;
    }
    if (finished_at && packets - finished_at >= 20) {
        assert(left_target == 0 && right_target == 0);
        longjmp(finished, 1);
    }
}

void Kobuki_Drive(int16_t speed)
{
    assert(speed == STOP || speed == -DEPOSIT_REVERSE_SPEED || speed == DEPOSIT_APPROACH_SPEED);
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
    assert(speed == DEPOSIT_SCAN_SPEED_MM_S || speed == -DEPOSIT_SCAN_SPEED_MM_S);
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
    state = ST_IDLE;
    gap = largest_gap = initial_gap;
    gyro = left_speed = right_speed = left_target = right_target = 0;
    sonar = pending_sonar = 5000;
    packets = reverse_commands = forward_commands = injected_at = finished_at = 0;
    led = OFF;
    injection = event;
    injection_state = event_state;
    saw_reverse = saw_approach = false;
    disturbed = false;
    add_noise = noise;
    if (setjmp(finished) == 0) {
        firmware_main();
        assert(!"firmware loop returned");
    }
    assert(state == ST_IDLE);
}

int main(void)
{
    assert(DEPOSIT_TEST_MODE);
    const double ranges[] = {300, 350, 409, 410, 420, 430, 431, 520, 800};
    for (unsigned i = 0; i < sizeof ranges / sizeof ranges[0]; i++) {
        for (unsigned noise = 0; noise < 2; noise++) {
            run(ranges[i], INJECT_NONE, ST_IDLE, noise != 0);
            assert(led == GREEN && robot.deposit_rod.detected);
            // The real gap also includes the injected ±3 mm measurement error and ADC rounding.
            double measurement_error = noise ? 3.5 : 0.5;
            assert(gap >= DEPOSIT_TARGET_RANGE_MM - DEPOSIT_RANGE_TOLERANCE_MM - measurement_error);
            assert(gap <= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM + measurement_error);
            assert(robot.deposit_range_mm >= DEPOSIT_TARGET_RANGE_MM - DEPOSIT_RANGE_TOLERANCE_MM);
            assert(robot.deposit_range_mm <= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM);
            if (ranges[i] <= 350) {
                assert(saw_reverse && saw_approach && reverse_commands && forward_commands);
                assert(largest_gap >= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_REVERSE_MARGIN_MM);
            }
            if (!noise && ranges[i] >= 410 && ranges[i] <= 430) {
                assert(!reverse_commands && !forward_commands);
            }
            if (ranges[i] >= 520) {
                assert(!reverse_commands && forward_commands);
            }
            printf("PASS initial %.0f mm%s: settled at %.1f mm (live %u mm)\n", ranges[i],
                   noise ? " with noise" : "", gap, robot.deposit_range_mm);
        }
    }

    run(800, INJECT_OVERSHOOT, ST_DEPOSIT_DISTANCE, false);
    assert(disturbed && saw_reverse && reverse_commands && forward_commands && led == GREEN);
    assert(robot.deposit_range_mm >= DEPOSIT_TARGET_RANGE_MM - DEPOSIT_RANGE_TOLERANCE_MM);
    assert(robot.deposit_range_mm <= DEPOSIT_TARGET_RANGE_MM + DEPOSIT_RANGE_TOLERANCE_MM);
    puts("PASS: a too-short settled reading repeats reverse-then-approach correction");

    const State phases[] = {ST_DEPOSIT_REVERSE, ST_DEPOSIT_APPROACH, ST_DEPOSIT_DISTANCE};
    for (unsigned phase = 0; phase < sizeof phases / sizeof phases[0]; phase++) {
        for (Injection event = INJECT_B1; event <= INJECT_NO_ECHO; event++) {
            run(phases[phase] == ST_DEPOSIT_APPROACH ? 800 : 350, event, phases[phase], false);
            assert(injected_at);
            assert(led != GREEN);
            if (event == INJECT_ZERO || event == INJECT_NO_ECHO) {
                assert(led == RED);
            }
            printf("PASS immediate stop: phase %u, event %u\n", phase, (unsigned)event);
        }
    }
    puts("PASS: 420 mm control, reverse past target then approach, tolerance, sensor latency/noise and stops");
    return 0;
}
