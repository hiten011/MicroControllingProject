#include <stdint.h>
#include <stdbool.h>

#define STM32G474xx

#include "stm32g4xx.h"
#include "uart.h"
#include "adc.h"
#include "ultrasonic.h"
#include "kobuki.h"
#include "fsm.h"
#include "fsm_states.h"

int main(void)
{
    UART_Init();
    ADC_Init();
    Ultrasonic_Init();

    FSM_Init(fsm_states, FSM_ST_COUNT, FSM_ST_IDLE);

    while (1) {
        // Kobuki_Read() waits for the next Kobuki packet, so this loop makes one Tick per packet.
        Kobuki_Read(&fsm_data.kobuki);
        FSM_Tick();
    }
}
