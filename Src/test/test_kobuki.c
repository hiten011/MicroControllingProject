#include <stdint.h>
#include "uart.h"
#include "kobuki.h"
#include "test.h"

// One Kobuki_Read() waits for one feedback packet (about 20 ms),
// so loop counts below are also the time base.
#define MOVE_CYCLES        50U  // about 1 s
#define SETTLE_CYCLES      10U  // let the robot come to rest
#define CLOSED_LOOP_CYCLES 400U // about 8 s timeout
#define LED_CYCLES         25U  // about 0.5 s per colour
#define RX_ATTEMPTS        5U

#define TEST_SPEED         ((int16_t)100) // mm / s
#define MIN_DISTANCE_TICKS 200            // about 17 mm
#define MIN_ANGLE_TICKS    200            // 2 deg

#define ROTATE_ANGLE_DEG   90
#define ANGLE_MIN_TICKS    7500           // 90 deg +/- 15 deg
#define ANGLE_MAX_TICKS    10500

#define DRIVE_DISTANCE_MM  100
#define DISTANCE_MIN_TICKS 1000           // 100 mm +/- 15 mm
#define DISTANCE_MAX_TICKS 1350

volatile Test_Kobuki_Results test_kobuki;

static Kobuki_Typedef kobuki;

static uint8_t Unsafe(void)
{
    return (uint8_t)((kobuki.bumper | kobuki.cliff | kobuki.wheeldrop) != 0U);
}

static void Stop_And_Settle(void)
{
    uint16_t i;

    for (i = 0; i < SETTLE_CYCLES; i++) {
        Kobuki_Read(&kobuki);
        Kobuki_Drive(STOP);
    }
}

// Send a motor command every cycle, then stop.
// Returns 1 if a bumper, cliff or wheel drop ended the run early.
static uint8_t Run(void (*command)(int16_t), int16_t speed, uint16_t cycles)
{
    uint16_t i;
    uint8_t blocked = 0;

    for (i = 0; i < cycles; i++) {
        Kobuki_Read(&kobuki);
        if (Unsafe()) {
            blocked = 1;
            break;
        }
        command(speed);
    }
    Stop_And_Settle();

    return blocked;
}

// Base control packet built by hand, to test Kobuki_Tx without Kobuki_Drive
static void Raw_Drive(int16_t speed)
{
    uint8_t payload[] = {0x01, 0x04, (uint8_t)speed, (uint8_t)(speed >> 8), 0x00, 0x00};

    Kobuki_Tx(payload, sizeof(payload));
}

// Signed angle change in ticks, corrected for the +/-180 deg wrap
static int32_t Angle_Delta(int16_t before, int16_t after)
{
    int32_t delta = (int32_t)after - (int32_t)before;

    if (delta > 18000) {
        delta -= 36000;
    } else if (delta < -18000) {
        delta += 36000;
    }
    return delta;
}

static void Show_LEDs(Kobuki_LED_Enum colour)
{
    uint16_t i;

    for (i = 0; i < LED_CYCLES; i++) {
        Kobuki_Read(&kobuki);
        Kobuki_Update_LEDs(colour, colour);
    }
}

static void Test_Rx(void)
{
    uint8_t feedback[SIZE_FEEDBACK];
    uint8_t i;
    uint8_t checksum_ok = 0;

    // Kobuki_Rx: a packet arrives with a valid checksum
    for (i = 0; i < RX_ATTEMPTS; i++) {
        if (Kobuki_Rx(feedback, SIZE_FEEDBACK) == 0U) {
            checksum_ok = 1;
            break;
        }
    }
    test_kobuki.rx = TEST_CHECK(checksum_ok == 1U);
}

static void Test_Read(void)
{
    uint8_t i;

    // Kobuki_Read: struct is overwritten with valid sensor flags.
    // 0xFF can never come from the Kobuki, flags only use the low 3 bits.
    kobuki.bumper = 0xFFU;
    kobuki.wheeldrop = 0xFFU;
    kobuki.cliff = 0xFFU;
    kobuki.button = 0xFFU;

    for (i = 0; i < RX_ATTEMPTS; i++) {
        Kobuki_Read(&kobuki);
        if (kobuki.bumper != 0xFFU) {
            break;
        }
    }
    test_kobuki.read = TEST_CHECK(
        (kobuki.bumper <= 0x07U) &&
        (kobuki.wheeldrop <= 0x03U) &&
        (kobuki.cliff <= 0x07U) &&
        (kobuki.button <= 0x07U)
    );
}

static void Test_Tx(void)
{
    uint16_t start;
    uint8_t blocked;

    // Kobuki_Tx: raw forward packet makes the encoder count up
    start = kobuki.distance;
    blocked = Run(Raw_Drive, TEST_SPEED, MOVE_CYCLES);
    test_kobuki.tx_ticks = (int16_t)(kobuki.distance - start);
    test_kobuki.tx = TEST_CHECK(
        (blocked == 0U) && (test_kobuki.tx_ticks > MIN_DISTANCE_TICKS)
    );
}

static void Test_Drive(void)
{
    uint16_t start;
    uint8_t blocked;

    // Kobuki_Drive: reverse makes the encoder count down (back to the start)
    start = kobuki.distance;
    blocked = Run(Kobuki_Drive, (int16_t)-TEST_SPEED, MOVE_CYCLES);
    test_kobuki.drive_ticks = (int16_t)(kobuki.distance - start);
    test_kobuki.drive = TEST_CHECK(
        (blocked == 0U) && (test_kobuki.drive_ticks < -MIN_DISTANCE_TICKS)
    );
}

static void Test_Rotate(void)
{
    int16_t start;
    uint8_t blocked;

    // Kobuki_Rotate: +ve speed turns CCW (angle up), -ve turns back CW
    start = kobuki.angle;
    blocked = Run(Kobuki_Rotate, SEARCH_SPEED, MOVE_CYCLES);
    test_kobuki.rotate_ccw_ticks = Angle_Delta(start, kobuki.angle);

    start = kobuki.angle;
    blocked |= Run(Kobuki_Rotate, (int16_t)-SEARCH_SPEED, MOVE_CYCLES);
    test_kobuki.rotate_cw_ticks = Angle_Delta(start, kobuki.angle);

    test_kobuki.rotate = TEST_CHECK(
        (blocked == 0U) &&
        (test_kobuki.rotate_ccw_ticks > MIN_ANGLE_TICKS) &&
        (test_kobuki.rotate_cw_ticks < -MIN_ANGLE_TICKS)
    );
}

static void Test_Rotate_Angle(void)
{
    int16_t start;
    uint16_t i;
    uint8_t cleared;
    uint8_t complete = 0;

    // Kobuki_Rotate_Angle: turns 90 deg and raises rotation_complete
    start = kobuki.angle;
    Kobuki_Rotate_Angle_Setpoint(kobuki.angle, ROTATE_ANGLE_DEG);
    Kobuki_Read(&kobuki);
    cleared = (uint8_t)(kobuki.rotation_complete == 0U);

    for (i = 0; i < CLOSED_LOOP_CYCLES; i++) {
        Kobuki_Read(&kobuki);
        if (Unsafe()) {
            break;
        }
        if (kobuki.rotation_complete == 1U) {
            complete = 1;
            break;
        }
        Kobuki_Rotate_Angle(kobuki.angle);
    }
    Stop_And_Settle();

    test_kobuki.rotate_angle_ticks = Angle_Delta(start, kobuki.angle);
    test_kobuki.rotate_angle = TEST_CHECK(
        (complete == 1U) &&
        (test_kobuki.rotate_angle_ticks >= ANGLE_MIN_TICKS) &&
        (test_kobuki.rotate_angle_ticks <= ANGLE_MAX_TICKS)
    );

    // Kobuki_Rotate_Angle_Setpoint: a new setpoint clears rotation_complete.
    // Only provable after a completed rotation, otherwise left as TEST_NOT_RUN.
    if (complete == 1U) {
        Kobuki_Rotate_Angle_Setpoint(kobuki.angle, 0);
        Kobuki_Read(&kobuki);
        test_kobuki.rotate_angle_setpoint = TEST_CHECK(
            (cleared == 1U) && (kobuki.rotation_complete == 0U)
        );
    }
}

static void Test_Drive_Distance(void)
{
    uint16_t start;
    uint16_t i;
    uint8_t cleared;
    uint8_t complete = 0;

    // Kobuki_Drive_Distance: drives 100 mm and raises distance_complete
    start = kobuki.distance;
    Kobuki_Drive_Distance_Setpoint(kobuki.distance, DRIVE_DISTANCE_MM);
    Kobuki_Read(&kobuki);
    cleared = (uint8_t)(kobuki.distance_complete == 0U);

    for (i = 0; i < CLOSED_LOOP_CYCLES; i++) {
        Kobuki_Read(&kobuki);
        if (Unsafe()) {
            break;
        }
        if (kobuki.distance_complete == 1U) {
            complete = 1;
            break;
        }
        Kobuki_Drive_Distance(kobuki.distance);
    }
    Stop_And_Settle();

    test_kobuki.drive_distance_ticks = (int16_t)(kobuki.distance - start);
    test_kobuki.drive_distance = TEST_CHECK(
        (complete == 1U) &&
        (test_kobuki.drive_distance_ticks >= DISTANCE_MIN_TICKS) &&
        (test_kobuki.drive_distance_ticks <= DISTANCE_MAX_TICKS)
    );

    // Kobuki_Drive_Distance_Setpoint: a new setpoint clears distance_complete.
    // Only provable after a completed drive, otherwise left as TEST_NOT_RUN.
    if (complete == 1U) {
        Kobuki_Drive_Distance_Setpoint(kobuki.distance, 0);
        Kobuki_Read(&kobuki);
        test_kobuki.drive_distance_setpoint = TEST_CHECK(
            (cleared == 1U) && (kobuki.distance_complete == 0U)
        );
    }
}

static void Test_Update_LEDs(void)
{
    // Kobuki_Update_LEDs: both LEDs go red, green, yellow, off.
    // Nothing to read back, so PASS only means the packets were sent:
    // confirm the colours by eye.
    Show_LEDs(RED);
    Show_LEDs(GREEN);
    Show_LEDs(YELLOW);
    Show_LEDs(OFF);
    test_kobuki.update_leds = TEST_PASS;
}

// Needs UART_Init() first: Test_UART() runs before this one.
// The robot moves: about 10 cm forward and back, turns, then 90 deg and 10 cm.
void Test_Kobuki(void)
{
    Test_Rx();
    Test_Read();
    Test_Tx();
    Test_Drive();
    Test_Rotate();
    Test_Rotate_Angle();
    Test_Drive_Distance();
    Test_Update_LEDs();
}
