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
