#include <stdint.h>
#include "stm32g474xx.h"
#include "ultrasonic.h"
#include "test.h"

volatile Test_Ultrasonic_Results test_ultrasonic;

// Needs ADC_Init() first: Test_ADC() runs before this one
void Test_Ultrasonic(void)
{
    uint16_t range;
    uint8_t expected;
    US_Typedef object = {0xFFU, 0xFFFFU}; // values Ultrasonic_Detect can never write

    // Ultrasonic_Init: GPIOA clock on, PA0 analogue, no pull resistors
    Ultrasonic_Init();
    test_ultrasonic.init = TEST_CHECK(
        ((RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN) != 0U) &&
        ((GPIOA->MODER & GPIO_MODER_MODE0) == GPIO_MODER_MODE0) &&
        ((GPIOA->PUPDR & GPIO_PUPDR_PUPD0) == 0U)
    );

    // Ultrasonic_Read: range never above the maximum
    range = Ultrasonic_Read();
    test_ultrasonic.range_mm = range;
    test_ultrasonic.read = TEST_CHECK(range <= ULTRASONIC_MAX_RANGE_MM);

    // Ultrasonic_Detect: null pointer ignored, struct filled, flag matches range
    Ultrasonic_Detect(0);
    Ultrasonic_Detect(&object);
    test_ultrasonic.detected = object.detected;
    expected = (object.range_mm < ULTRASONIC_DETECT_RANGE_MM) ? 1U : 0U;
    test_ultrasonic.detect = TEST_CHECK(
        (object.range_mm <= ULTRASONIC_MAX_RANGE_MM) &&
        (object.detected == expected)
    );
}
