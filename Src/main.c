#include <stdint.h>
#include <stdbool.h>

#define STM32G474xx

#include "stm32g4xx.h"
#include "uart.h"
#include "kobuki.h"
#include "robot_control.h"

int main(void)
{
    Kobuki_Typedef kobuki;
    Kobuki_LED_Enum led_1 = GREEN;

    UART_Init();
    Robot_Init();
    Kobuki_Update_LEDs(led_1, OFF);

    // Move forward 1 metre
    Robot_Move(1000, 200);

    while (1)
    {
        // Update movement and bumper sensors
        Robot_Update(&kobuki);

        // Stop if a bumper is pressed
        if (Robot_BumperPressed())
        {
            Robot_Stop();
            led_1 = RED;
            Kobuki_Update_LEDs(led_1, OFF);
        }
    }
}