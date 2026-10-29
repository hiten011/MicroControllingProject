#include <stdint.h>
#include <stdlib.h>

#define STM32G474xx
#include "stm32g4xx.h"
#include "uart.h"
#include "kobuki.h"

// Parameters for distance and angle calculation
// These can be placed in kobuki.h

#define PI                   (3.14159)
#define WHEEL_RADIUS         (35.0)  // mm
#define REVOLUTIONS_PER_TICK ((132.0 / 6545.0) / 52.0)
#define MM_PER_TICK          (2.0 * PI * WHEEL_RADIUS * REVOLUTIONS_PER_TICK)
#define DEG_PER_TICK         (1.0 / 100.0)
#define GAIN_ROTATE          (1.0 / 0.90)
#define GAIN_DRIVE           (100.0 / 100.0)
#define sign(x)              ((int8_t)((x) >= 0 ? 1 : -1))

// Static variables and flags
// Keep these in kobuki.c

static volatile int32_t setpoint_distance_tick;
static volatile int16_t setpoint_angle_deg;
static uint8_t rotation_complete = 0;
static uint8_t distance_complete = 0;

void Kobuki_Tx(uint8_t *payload, uint8_t size_payload)
{
    uint8_t checksum = 0;
    uint8_t i;

    UART_Tx(0xAA);          // Send header 0
    UART_Tx(0x55);          // Send header 1
    UART_Tx(size_payload);  // Send size of payload in bytes

    checksum ^= size_payload;  // Calculate running checksum

    // Send each byte of payload one-by-one
    for (i = 0; i < size_payload; i++) {
        UART_Tx(payload[i]);
        checksum ^= payload[i];
    }

    UART_Tx(checksum);  // Send checksum
}

uint8_t Kobuki_Rx(uint8_t *feedback, uint32_t size_feedback)
{
    uint8_t feedback_header = 0;
    uint8_t size_payload = 0;
    uint8_t checksum = 0;
    uint8_t rx_checksum = 0;
    uint32_t i;

    // 1. Find the valid 0xAA 0x55 sequence
    while (1) {
        feedback_header = UART_Rx();

        if (feedback_header == 0xAA) {
            feedback_header = UART_Rx();

            if (feedback_header == 0x55) {
                break;  // Valid start found
            }
        }
    }

    // 2. Read Payload Length
    size_payload = UART_Rx();

    checksum ^= size_payload;

    // 3. Determine safe boundary to prevent array overflow
    uint32_t limit = (size_payload < size_feedback) ? size_payload : size_feedback;

    // 4. Read payload into feedback array
    for (i = 0; i < limit; i++) {
        feedback[i] = UART_Rx();
        checksum ^= feedback[i];
    }

    // 5. Read and discard remaining payload
    for (i = limit; i < size_payload; i++) {
        checksum ^= UART_Rx();
    }

    // 6. Read received checksum
    rx_checksum = UART_Rx();           // checksum from kobuki-final byte
    return (checksum != rx_checksum);  // 0 = Success, 1 = Error
}

void Kobuki_Read(Kobuki_Typedef *kobuki)
{
    uint16_t distance_LSB;
    uint16_t distance_MSB;
    int16_t angle_LSB;
    int16_t angle_MSB;

    // Init buffer & checksum
    uint8_t kobuki_rx_buffer[SIZE_FEEDBACK] = {0};
    uint8_t checksum_result;

    // Read Kobuki Tx data stream
    checksum_result = Kobuki_Rx(kobuki_rx_buffer, SIZE_FEEDBACK);

    // If checksum checks out, update sensor variables from buffer
    if (checksum_result == 0) {
        distance_LSB = kobuki_rx_buffer[DIST_LSB];
        angle_LSB = kobuki_rx_buffer[ANGLE_LSB];

        distance_MSB = (uint16_t)((uint16_t)kobuki_rx_buffer[DIST_MSB] << 8);

        angle_MSB = (int16_t)((int16_t)kobuki_rx_buffer[ANGLE_MSB] << 8);

        kobuki->bumper = kobuki_rx_buffer[BUMPER];
        kobuki->wheeldrop = kobuki_rx_buffer[WHEEL_DROP];
        kobuki->cliff = kobuki_rx_buffer[CLIFF];
        // The Kobuki's own flag has a fixed threshold, so add our own check on the raw readings.
        // Bits: 0x01 right, 0x02 centre, 0x04 left.
        if (kobuki_rx_buffer[CLIFF_ID] == 0x05U) {
            for (uint8_t s = 0U; s < 3U; s++) {
                kobuki->cliff_raw[s] = (uint16_t)kobuki_rx_buffer[CLIFF_RAW + (2U * s)] |
                                       (uint16_t)((uint16_t)kobuki_rx_buffer[CLIFF_RAW + (2U * s) + 1U] << 8);
                if (kobuki->cliff_raw[s] < CLIFF_RAW_LIMIT) {
                    kobuki->cliff |= (uint8_t)(1U << s);
                }
            }
        }
        kobuki->button = kobuki_rx_buffer[BUTTON];
        kobuki->distance = distance_LSB | distance_MSB;
        kobuki->angle = angle_LSB | angle_MSB;
    }
    // Otherwise, do not update sensor variables
    else {
    }

    // Always update completion flags
    kobuki->rotation_complete = rotation_complete;
    kobuki->distance_complete = distance_complete;
}

void Kobuki_Drive(int16_t speed)
{
    uint8_t speed_lsb = 0;
    uint8_t speed_msb = 0;

    // Construct payload
    uint8_t payload[] = {0x01, 0x04, speed_lsb, speed_msb, 0x00, 0x00};  /// 4th one is important

    // Saturation to avoid going over speed limit
    if (speed > SPEED_LIMIT) {
        speed = SPEED_LIMIT;
    } else if (speed < -SPEED_LIMIT) {
        speed = (~SPEED_LIMIT) + 1;
    }

    // Split LSB & MSB for payload
    payload[2] = (uint8_t)speed;
    payload[3] = (uint8_t)(speed >> 8);

    Kobuki_Tx(payload, sizeof(payload));
}

void Kobuki_Rotate(int16_t speed)
{
    uint8_t speed_lsb = 0;
    uint8_t speed_msb = 0;
    // Construct payload
    uint8_t payload[] = {0x01, 0x04, 0x00, 0x00, 0x01, 0x00};
    // saturation to avoid going over speed limit
    if (speed > SPEED_LIMIT) {
        speed = SPEED_LIMIT;
    } else if (speed < -SPEED_LIMIT) {
        speed = (~SPEED_LIMIT) + 1;
    }
    // split lsb & msb for payload
    payload[2] = (uint8_t)speed;
    payload[3] = (uint8_t)(speed >> 8);
    Kobuki_Tx(payload, sizeof(payload));
}

void Kobuki_Rotate_Angle_Setpoint(int16_t current_angle_tick, int16_t rot_angle_deg)
{
    volatile int32_t init_angle;

    rotation_complete = 0;

    // Initial angle
    init_angle =
        (int32_t)((double)(current_angle_tick + (int32_t)18000) * DEG_PER_TICK);  // [-18000, 18000] -> [0, 360] degrees

    // Setpoint angle
    setpoint_angle_deg = (init_angle + rot_angle_deg + 360) % 360;
    // [0, 360]
}

void Kobuki_Rotate_Angle(int16_t current_angle_tick)
{
    volatile int16_t speed;
    volatile int16_t error;
    volatile int32_t current_angle_deg;

    current_angle_deg = (int32_t)((double)(current_angle_tick + (int32_t)18000) * DEG_PER_TICK);

    error = (int16_t)(setpoint_angle_deg - current_angle_deg);

    // Check for non-optimal rotation angle
    if (abs(error) > 180) {
        // Always rotate with the minimum angle, e.g. +270 -> -90.
        // Introduces singularity: loses sign and truncates error.
        error = -sign(error) * (abs(error) % 180);

        // Special cases in circular buffer: offset to correct error.
        if ((error < 0) && (current_angle_deg < setpoint_angle_deg)) {
            error = -(error + 180);
        } else if ((error > 0) && (current_angle_deg > setpoint_angle_deg)) {
            error = -(error - 180);
            // Does this make sense? Is there a simpler way?
        }
    }

    if (abs(error) <= 0) {
        Kobuki_Drive(STOP); // Added to increase accuracy when turning.
        rotation_complete = 1;
    } else {
        speed = (sign(error) * MIN_SPEED) + (int16_t)(GAIN_ROTATE * error);
        Kobuki_Rotate(speed);
    }
}

bool Kobuki_IsCliffDetected(const Kobuki_Typedef *kobuki, uint8_t side_mask)
{
    return (kobuki->cliff & side_mask) != 0;
}

int32_t Kobuki_Distance_Delta_Mm(uint16_t start_tick, uint16_t current_tick)
{
    int16_t delta_ticks = (int16_t)(current_tick - start_tick);
    return (int32_t)((double)delta_ticks * MM_PER_TICK);
}

void Kobuki_Drive_Distance_Setpoint(uint16_t current_distance_tick, int16_t distance)
{
    distance_complete = 0;
    setpoint_distance_tick = current_distance_tick + (int32_t)(distance / MM_PER_TICK);
}

void Kobuki_Drive_Distance(uint16_t current_distance_tick)
{
    volatile int16_t speed;
    volatile int16_t error;
    volatile uint16_t setpoint_alias;

    // Standard condition
    error = (int16_t)((double)(setpoint_distance_tick - current_distance_tick) * MM_PER_TICK);
    setpoint_alias = (uint16_t)((setpoint_distance_tick + 0x10000) % 0x10000);

    if (((setpoint_distance_tick > 0xFFFF) && (current_distance_tick < setpoint_alias)) ||
        ((setpoint_distance_tick < 0) && (current_distance_tick > setpoint_alias))) {
        // Alternate condition
        error = (int16_t)((double)(setpoint_alias - current_distance_tick) * MM_PER_TICK);
    }

    if (abs(error) <= 2) {
        distance_complete = 1;
    } else {
        speed = (sign(error) * MIN_SPEED) + (int16_t)(GAIN_DRIVE * error);

        Kobuki_Drive(speed);
    }
}

// This wrapper is based on the General Purpose Output
// information in the Kobuki bytestream document
#define EXTERN_PWR 0x30
void Kobuki_Update_LEDs(Kobuki_LED_Enum led_1_colour, Kobuki_LED_Enum led_2_colour)
{
    // Split LSB and MSB for payload
    uint8_t GPO_lsb = EXTERN_PWR;  // Keep 5 V and 3.3 V on

    uint8_t GPO_msb = led_1_colour | (uint8_t)(led_2_colour << 2);

    uint8_t payload[] = {0x0C, 0x02, GPO_lsb, GPO_msb};

    Kobuki_Tx(payload, sizeof(payload));
}