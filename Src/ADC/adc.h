#ifndef ADC_H
#define ADC_H

#include <stdint.h>

// ADC1 input channel 1 (PA0).
#define ADC_CHANNEL_1 ((uint8_t)1U)

// Enable ADC1 and calibrate it. Call once at start-up.
void ADC_Init(void);
// Run one blocking conversion on channel 1..18. Returns the 12-bit result, or 0 for a bad channel.
uint16_t ADC_Read(uint8_t channel);

#endif
