#include <stdint.h>
#include "stm32g474xx.h"
#include "uart.h"
#include "test.h"

#define BAUD_115200_AT_16MHZ 139U
#define TX_TIMEOUT_COUNT     100000U
#define RX_MAX_BYTES         200U // longer than one Kobuki feedback packet

volatile Test_UART_Results test_uart;

void Test_UART(void)
{
    uint32_t i;
    uint8_t byte;
    uint8_t previous = 0;
    uint8_t header_found = 0;

    // UART_Init: baud rate, USART3 enabled with Tx and Rx, PC10/PC11 on AF7
    UART_Init();
    test_uart.init = TEST_CHECK(
        (USART3->BRR == BAUD_115200_AT_16MHZ) &&
        ((USART3->CR1 & (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE)) ==
            (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE)) &&
        ((GPIOC->MODER & (GPIO_MODER_MODE10 | GPIO_MODER_MODE11)) ==
            (GPIO_MODER_MODE10_1 | GPIO_MODER_MODE11_1)) &&
        ((GPIOC->AFR[1] & (GPIO_AFRH_AFSEL10 | GPIO_AFRH_AFSEL11)) ==
            ((7U << GPIO_AFRH_AFSEL10_Pos) | (7U << GPIO_AFRH_AFSEL11_Pos)))
    );

    // UART_Tx: byte leaves the shift register (Transmission Complete)
    UART_Tx(0x00U);
    for (i = 0; i < TX_TIMEOUT_COUNT; i++) {
        if ((USART3->ISR & USART_ISR_TC) != 0U) {
            break;
        }
    }
    test_uart.tx = TEST_CHECK((USART3->ISR & USART_ISR_TC) != 0U);

    // UART_Rx: Kobuki packet header 0xAA 0x55 arrives.
    // Blocks here forever if the Kobuki is off or unplugged.
    for (i = 0; i < RX_MAX_BYTES; i++) {
        byte = UART_Rx();
        if ((previous == 0xAAU) && (byte == 0x55U)) {
            header_found = 1;
            break;
        }
        previous = byte;
    }
    test_uart.rx = TEST_CHECK(header_found == 1U);
}
