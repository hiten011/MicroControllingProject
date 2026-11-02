#ifndef KOBUKI_H
#define KOBUKI_H

#include <stdbool.h>
#include <stdint.h>

// Latest sensor values from one Kobuki feedback packet, plus the driver's move-complete flags.
typedef struct {
    uint8_t cliff;
    uint16_t cliff_raw[3];  // right, centre, left PSD readings
    uint8_t bumper;
    uint8_t wheeldrop;
    uint8_t button;
    uint16_t distance;
    int16_t angle;
    uint8_t rotation_complete;
    uint8_t distance_complete;
} Kobuki_Typedef;

// LED colours for Kobuki_Update_LEDs().
typedef enum { OFF, RED, GREEN, YELLOW } Kobuki_LED_Enum;

// Bytes of feedback payload kept, up to the end of the 0x05 cliff sub-payload.
#define SIZE_FEEDBACK 39

// Byte offsets into the feedback payload.
#define BUMPER     4
#define WHEEL_DROP 5
#define CLIFF      6
#define DIST_LSB   7  // only needs 1 wheel
#define DIST_MSB   8
#define BUTTON     13
#define ANGLE_LSB  24  // only needs 1 wheel
#define ANGLE_MSB  25
#define CLIFF_ID   31  // sub-payload header, 0x05
#define CLIFF_RAW  33  // right, centre, left as uint16 LSB first

// A raw cliff reading under this counts as a ledge. Calibrate on the arena: read cliff_raw over floor and ledge.
#define CLIFF_RAW_LIMIT 1100U

// Speed limits and the stop command, all in mm/s.
#define SPEED_LIMIT (int16_t)150
#define STOP        ((int16_t)0)
#define MIN_SPEED   ((int16_t)50)

// Receive one packet into feedback. Returns 0 on success, 1 on a checksum error.
uint8_t Kobuki_Rx(uint8_t *feedback, uint32_t size_feedback);
// Frame payload with header, length and XOR checksum, then send it.
void Kobuki_Tx(uint8_t *payload, uint8_t size_payload);
// Receive a packet and update kobuki when its checksum is good.
void Kobuki_Read(Kobuki_Typedef *kobuki);
// Drive forward or backward in mm/s (positive is forward), clamped to SPEED_LIMIT.
void Kobuki_Drive(int16_t speed);
// Rotate at the given speed (positive is anticlockwise), clamped to SPEED_LIMIT.
void Kobuki_Rotate(int16_t speed);

// Set a turn target of rot_angle_deg from the current gyro angle.
void Kobuki_Rotate_Angle_Setpoint(int16_t current_angle_tick, int16_t rot_angle_deg);
// Step the turn towards its target. Sets rotation_complete when it arrives.
void Kobuki_Rotate_Angle(int16_t current_angle_tick);
// Set a drive target of distance mm from the current encoder count.
void Kobuki_Drive_Distance_Setpoint(uint16_t current_distance_tick, int16_t distance);
// Step the drive towards its target. Sets distance_complete when it arrives.
void Kobuki_Drive_Distance(uint16_t current_distance_tick);
// True when any cliff sensor in side_mask (right 0x01, centre 0x02, left 0x04) sees the ledge.
bool Kobuki_IsCliffDetected(const Kobuki_Typedef *kobuki, uint8_t side_mask);

// Set the two LEDs. Keeps the 5 V and 3.3 V outputs on.
void Kobuki_Update_LEDs(Kobuki_LED_Enum led_1_colour, Kobuki_LED_Enum led_2_colour);

#endif
