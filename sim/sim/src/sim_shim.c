/*
 * sim_shim.c - debugger-friendly replacement for uart.c and adc.c.
 *
 * Built only by "build.bat shim". The register-level build traps every
 * USART3/ADC1 access with access-violation + single-step exceptions,
 * which confuses gdb. This build swaps the two lowest drivers for direct
 * calls into the simulator so breakpoints in the FSM work normally.
 * Everything above them (kobuki.c, Ultrasonic.c, Navigation/, main.c)
 * is still the student's code, unchanged.
 */
#include <stdint.h>
#include "sim.h"

extern int     periph_shim;
uint8_t        periph_shim_uart_rx(void);
void           periph_shim_uart_tx(uint8_t b);

void     UART_Init(void)           { periph_shim = 1; }
uint8_t  UART_Rx(void)             { return periph_shim_uart_rx(); }
void     UART_Tx(uint8_t byte)     { periph_shim_uart_tx(byte); }

void     ADC_Init(void)            { }
uint16_t ADC_Read(uint8_t channel) { return sensor_adc(channel); }
