#include <stdint.h>
#include "stm32g474xx.h"
#include "adc.h"
#include "test.h"

#define ADC_MAX_VALUE 4095U

volatile Test_ADC_Results test_adc;

void Test_ADC(void)
{
    uint16_t raw;

    // ADC_Init: ADC1 enabled and ready
    ADC_Init();
    test_adc.init = TEST_CHECK(
        ((ADC1->CR & ADC_CR_ADEN) != 0U) &&
        ((ADC1->ISR & ADC_ISR_ADRDY) != 0U)
    );

    // ADC_Read: 12-bit result, invalid channels return 0
    raw = ADC_Read(ADC_CHANNEL_1);
    test_adc.raw = raw;
    test_adc.read = TEST_CHECK(
        (raw <= ADC_MAX_VALUE) &&
        (ADC_Read(0U) == 0U) &&
        (ADC_Read(19U) == 0U)
    );
}
