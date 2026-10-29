#ifndef ULTRASONIC_H

#define ULTRASONIC_H
#define ULTRASONIC_DETECT_RANGE_MM 750 //750U
#define ULTRASONIC_MAX_RANGE_MM    5000 //5000U

#include <stdint.h>

typedef struct {
    uint8_t detected;
    uint16_t range_mm;
} US_Typedef;

void Ultrasonic_Init(void);
uint16_t Ultrasonic_Read(void);
void Ultrasonic_Detect(US_Typedef *object);

#endif
