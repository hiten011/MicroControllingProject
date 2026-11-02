#include <stdint.h>
#define STM32G474xx
#include "stm32g4xx.h"

void UART_Init(void)
{
    // Enable peripheral clocks.
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN;

    // Route HSI16 clock to USART3
    RCC->CCIPR &= ~RCC_CCIPR_USART3SEL;
    RCC->CCIPR |= (2U << RCC_CCIPR_USART3SEL_Pos);

    // Configure PC10 (TX) and PC11 (RX) as alternate function.
    GPIOC->MODER &= ~(GPIO_MODER_MODE10 | GPIO_MODER_MODE11);
    GPIOC->MODER |= (GPIO_MODER_MODE10_1 | GPIO_MODER_MODE11_1);

    // Route AF7 (USART3) to PC10 and PC11.
    GPIOC->AFR[1] &= ~(GPIO_AFRH_AFSEL10 | GPIO_AFRH_AFSEL11);

    GPIOC->AFR[1] |= ((7U << GPIO_AFRH_AFSEL10_Pos) | (7U << GPIO_AFRH_AFSEL11_Pos));

    // Configure USART3.
    USART3->CR1 &= ~USART_CR1_UE;  // Turn off to allow configuration

    // 16 MHz / 139 is about 115200 baud.
    USART3->BRR = 139;

    // Enable the transmitter and receiver.
    USART3->CR1 |= USART_CR1_TE | USART_CR1_RE;

    // Enable USART3.
    USART3->CR1 |= USART_CR1_UE;
}

void UART_Tx(uint8_t byte)
{
    // Wait until the transmit data register is empty.
    while (!(USART3->ISR & USART_ISR_TXE)) {}

    // Writing TDR starts the transmission.
    USART3->TDR = byte;
}

uint8_t UART_Rx(void)
{
    // Clear an overrun error so reception can continue.
    if (USART3->ISR & USART_ISR_ORE) {
        USART3->ICR = USART_ICR_ORECF;
    }

    // Wait until a byte has been received.
    while (!(USART3->ISR & USART_ISR_RXNE)) {}

    // Reading RDR clears the receive flag.
    return (uint8_t)(USART3->RDR & 0xFFU);
}
