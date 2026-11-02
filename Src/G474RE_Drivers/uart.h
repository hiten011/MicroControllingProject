#ifndef UART_H
#define UART_H

#include <stdint.h>
// Set up USART3 on PC10 (TX) and PC11 (RX) at 115200 baud for the Kobuki.
void UART_Init(void);
// Block until a byte arrives and return it.
uint8_t UART_Rx(void);
// Block until the transmitter is free, then send one byte.
void UART_Tx(uint8_t byte);
#endif
