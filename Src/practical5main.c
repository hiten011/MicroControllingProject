#include <stdint.h>
#include "stm32g474xx.h"
#define STM32G474xx
#include "uart.h"
#include "kobuki.h"

int main(void)
{
    UART_Init();

    // Structure containing Kobuki sensor readings
    Kobuki_Typedef my_kobuki = {0};

    // 0 = Stopped (Safe state), 1 = Running
    uint8_t is_running = 0;
    uint8_t is_rotating = 0;

    while (1)
    {
        // 1. Read the latest sensor data
        Kobuki_Read(&my_kobuki);

        // 2. Evaluate Start/Stop triggers

        // DRIVE STATE - Button 0
        if (my_kobuki.button == 0x01)
        {
            is_running = 1;
            is_rotating = 0;
        }

        // ROTATE STATE - Button 2
        else if (my_kobuki.button == 0x04)
        {
            is_rotating = 1;
            is_running = 0;
        }

        // Stop triggers take priority over start triggers
        // Button 1 OR bumper collision
        if ((my_kobuki.button == 0x02) || my_kobuki.bumper > 0)
        {
            is_running = 0;
            is_rotating = 0;
        }

        // 3. Command the motors based on current state

        if (is_running == 1)
        {
            Kobuki_Drive(100);
            Kobuki_Rotate(0);
        }
        else if (is_rotating == 1)
        {
            Kobuki_Rotate(100);
            Kobuki_Drive(0);
        }
        else
        {
            Kobuki_Drive(0);
            Kobuki_Rotate(0);
        }

        // 4. Control loop delay
        for (volatile int i = 0; i < 50000; i++)
        {
        }
    }
}