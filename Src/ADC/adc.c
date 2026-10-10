#include <stdint.h>
#include "stm32g474xx.h"
#include "adc.h"

void ADC_Init(void)
{
    /* ADC1 and ADC2 share the ADC12 clock. */
    RCC->AHB2ENR |= RCC_AHB2ENR_ADC12EN;

    /* Synchronous HCLK/1 ADC clock. */
    ADC12_COMMON->CCR &= ~ADC_CCR_CKMODE;
    ADC12_COMMON->CCR |= ADC_CCR_CKMODE_0;

    /* Exit deep power down and power up the ADC regulator. */
    ADC1->CR &= ~ADC_CR_DEEPPWD;
    ADC1->CR |= ADC_CR_ADVREGEN;
    for (volatile uint32_t i = 0U; i < 1000U; ++i) { __NOP(); }

    /* Single-ended calibration (ADC must be disabled). */
    ADC1->CR &= ~ADC_CR_ADCALDIF;
    ADC1->CR |= ADC_CR_ADCAL;
    while ((ADC1->CR & ADC_CR_ADCAL) != 0U) { }

    ADC1->ISR = ADC_ISR_ADRDY;
    ADC1->CR |= ADC_CR_ADEN;
    while ((ADC1->ISR & ADC_ISR_ADRDY) == 0U) { }
}

uint16_t ADC_Read(uint8_t channel)
{
    if ((channel < 1U) || (channel > 18U)) { return 0U; }

    /* One regular conversion (L = 0), channel in sequence slot 1. */
    ADC1->SQR1 = ((uint32_t)channel << ADC_SQR1_SQ1_Pos);

    /* Long sample time for the analogue sensor. */
    if (channel <= 9U) {
        uint32_t position = (uint32_t)channel * 3U;
        ADC1->SMPR1 &= ~(7UL << position);
        ADC1->SMPR1 |= (7UL << position);
    } else {
        uint32_t position = ((uint32_t)channel - 10U) * 3U;
        ADC1->SMPR2 &= ~(7UL << position);
        ADC1->SMPR2 |= (7UL << position);
    }

    ADC1->ISR = ADC_ISR_EOC | ADC_ISR_EOS;
    ADC1->CR |= ADC_CR_ADSTART;
    while ((ADC1->ISR & ADC_ISR_EOC) == 0U) { }
    return (uint16_t)(ADC1->DR & ADC_DR_RDATA);
}
