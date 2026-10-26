#include <stdint.h>
#include <stdbool.h>

#define STM32G474xx

#include "stm32g4xx.h"
#include "uart.h"
#include "adc.h"
#include "ultrasonic.h"
#include "kobuki.h"
#include "amu_fsm.h"

int main(void)
{
    Kobuki_Typedef kobuki = {0};
    AMU_Typedef amu;

    UART_Init();
    ADC_Init();
    Ultrasonic_Init();
    Kobuki_Update_LEDs(GREEN, OFF);
    AMU_Init(&amu);

    while (1) {
        // One FSM step per received Kobuki packet, sensors read at the top of the loop
        Kobuki_Read(&kobuki);
        AMU_Step(&amu, &kobuki);
    }
}
