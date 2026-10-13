#include <stdint.h>
#include "kobuki.h"
#include "test.h"

volatile uint8_t test_stage;
volatile Test_Result test_all;

static Test_Result All_Passed(void)
{
    return TEST_CHECK(
        (test_adc.init == TEST_PASS) &&
        (test_adc.read == TEST_PASS) &&
        (test_ultrasonic.init == TEST_PASS) &&
        (test_ultrasonic.read == TEST_PASS) &&
        (test_ultrasonic.detect == TEST_PASS) &&
        (test_uart.init == TEST_PASS) &&
        (test_uart.tx == TEST_PASS) &&
        (test_uart.rx == TEST_PASS) &&
        (test_kobuki.rx == TEST_PASS) &&
        (test_kobuki.read == TEST_PASS) &&
        (test_kobuki.tx == TEST_PASS) &&
        (test_kobuki.drive == TEST_PASS) &&
        (test_kobuki.rotate == TEST_PASS) &&
        (test_kobuki.rotate_angle_setpoint == TEST_PASS) &&
        (test_kobuki.rotate_angle == TEST_PASS) &&
        (test_kobuki.drive_distance_setpoint == TEST_PASS) &&
        (test_kobuki.drive_distance == TEST_PASS) &&
        (test_kobuki.update_leds == TEST_PASS)
    );
}

void Test_RunAll(void)
{
    Kobuki_Typedef kobuki;
    Kobuki_LED_Enum verdict;

    // Order matters: tests that need no Kobuki run first, because
    // UART_Rx and Kobuki_Read block forever if the Kobuki is off.
    // If it hangs, test_stage shows which test it is stuck in.
    test_stage = TEST_STAGE_ADC;
    Test_ADC();

    test_stage = TEST_STAGE_ULTRASONIC;
    Test_Ultrasonic();

    test_stage = TEST_STAGE_UART;
    Test_UART();

    test_stage = TEST_STAGE_KOBUKI;
    Test_Kobuki();

    test_all = All_Passed();
    test_stage = TEST_STAGE_DONE;

    // Green = every test passed, red = at least one did not
    verdict = (test_all == TEST_PASS) ? GREEN : RED;

    while (1) {
        Kobuki_Read(&kobuki);
        Kobuki_Update_LEDs(verdict, verdict);
    }
}
