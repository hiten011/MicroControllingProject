#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <stdint.h>

// Anything closer than this counts as a detected object, and readings are clamped to the max range.
#define ULTRASONIC_DETECT_RANGE_MM ((uint16_t)750U)
#define ULTRASONIC_MAX_RANGE_MM    ((uint16_t)5000U)

// Latest ultrasonic reading and whether it is inside the detect range.
typedef struct {
    uint8_t detected;
    uint16_t range_mm;
} US_Typedef;

// Put PA0 in analogue mode for the sensor. Call after ADC_Init().
void Ultrasonic_Init(void);
// Read the sensor and return the distance in mm, clamped to the max range.
uint16_t Ultrasonic_Read(void);
// Read the sensor into object and set its detected flag.
void Ultrasonic_Detect(US_Typedef *object);

#endif
