#include "my_stm_helper.h"
#include <stdint.h>

void Clock_Enable(PeripheralBus_t peripheral)
{
    switch (peripheral) {
        case GPIOA_BUS:
            RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
            break;
        case GPIOB_BUS:
            RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
            break;
        case GPIOC_BUS:
            RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
            break;
        case TIM16_BUS:
            RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
            break;
        case TIM17_BUS:
            RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
            break;
        case TIM3_BUS:
            RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
            break;
    }
}

void GPIO_InitPin(GPIO_TypeDef *port, uint8_t pin, GPIOMode_t mode, GPIOPull_t pull)
{
    port->MODER &= ~(0b11 << (pin * 2));
    port->MODER |= (mode << (pin * 2));

    // Configure Pull-Up/Pull-Down Resistor
    port->PUPDR &= ~(0b11 << (pin * 2));
    port->PUPDR |= (pull << (pin * 2));
}

void GPIO_InitPins(GPIO_TypeDef *port, uint8_t *pins, GPIOMode_t *mode, GPIOPull_t *pull, uint16_t len)
{
    for (int i = 0; i < len; i++) {
        GPIO_InitPin(port, pins[i], mode[i], pull[i]);
    }
}

void GPIO_InitPinsSameMode(GPIO_TypeDef *port, uint8_t *pins, GPIOMode_t mode, GPIOPull_t pull, uint16_t len)
{
    for (int i = 0; i < len; i++) {
        GPIO_InitPin(port, pins[i], mode, pull);
    }
}

// Fills AFR with the peripheral's AF number (per STM32G4 datasheet AF table).
// Caller is responsible for setting the pin to ALT mode.
void GPIO_ConnectPeripheral(GPIO_TypeDef *port, uint8_t pin, uint8_t af)
{
    if (pin < 8) {
        port->AFR[0] &= ~(0xFUL << (pin * 4));
        port->AFR[0] |= ((uint32_t)af << (pin * 4));
    } else {
        port->AFR[1] &= ~(0xFUL << ((pin - 8) * 4));
        port->AFR[1] |= ((uint32_t)af << ((pin - 8) * 4));
    }
}

void Timer_Init(TIM_TypeDef *timer, IRQn_Type irq_type, uint16_t psc, uint16_t arr, bool enable_interrupt,
                bool enable_arpe, bool force_update, bool enable_counter)
{
    timer->PSC = psc;
    timer->ARR = arr;

    if (enable_arpe) {
        timer->CR1 |= (1 << 7);  // ARPE: buffer ARR, only reload on update event
    }

    if (force_update) {
        timer->EGR |= (1 << 0);  // UG: force PSC/ARR to load now instead of on next overflow
    }

    if (enable_interrupt) {
        timer->DIER |= 1 << 0;
        NVIC_EnableIRQ(irq_type);
    }

    if (enable_counter) {
        timer->CR1 |= (1 << 0);
    }
}

void PWM_Init(TIM_TypeDef *timer, TimerChannel_t channel, TimerChannelMode_t mode, bool enable_counter)
{
    uint32_t OC2M, CC2S;

    switch (mode) {
        case TIM_MODE_OUTPUT_PWM1:
            OC2M = 0b110, CC2S = 0b00;
            break;
        default:
            OC2M = 0b000, CC2S = 0b00;
            break;
    }

    switch (channel) {
        case TIM_CHANNEL_2:
            // Set mode
            timer->CCMR1 &= ~(0b11 << 8);
            timer->CCMR1 |= (CC2S << 8);

            // set further
            timer->CCMR1 &= ~(0b111 << 12);
            timer->CCMR1 |= (OC2M << 12);

            timer->CCMR1 |= (1 << 11);  // enable preload
            timer->CCER |= (1 << 4);    // enable output
            break;

        case TIM_CHANNEL_3:
            // Set mode
            timer->CCMR2 &= ~(0b11 << 0);
            timer->CCMR2 |= (CC2S << 0);

            // set further
            timer->CCMR2 &= ~(0b111 << 4);
            timer->CCMR2 |= (OC2M << 4);

            timer->CCMR2 |= (1 << 3);  // enable preload
            timer->CCER |= (1 << 8);   // enable output
            break;

        default:
            break;
    }

    if (enable_counter) {
        timer->CR1 |= (1 << 0);
    }
}

// --- EXTI Wrappers ---
void EXTI_Init(GPIO_TypeDef *port, uint8_t pin, ExtiTrigger trigger, IRQn_Type irq_type)
{
    // 1. Establish the numerical port identifier
    uint32_t port_idx = 0;
    if (port == GPIOA) {
        port_idx = 0;
    } else if (port == GPIOB) {
        port_idx = 1;
    } else if (port == GPIOC) {
        port_idx = 2;
    }

    // 2. Configure the SYSCFG_EXTICRx Multiplexer
    // The STM32G4 allocates 4 bits per pin across 4 registers
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    uint8_t register_index = pin / 4;
    uint8_t bit_position = (pin % 4) * 4;
    SYSCFG->EXTICR[register_index] &= ~(0xFUL << bit_position);
    SYSCFG->EXTICR[register_index] |= (port_idx << bit_position);

    // 3. Configure Trigger Selection Registers : EXTI - > FTSR1 and EXTI - >
    // RTSR1
    // Clear existing trigger configurations
    EXTI->FTSR1 &= ~(1 << pin);
    EXTI->RTSR1 &= ~(1 << pin);
    if (trigger == TRIG_FALLING || trigger == TRIG_BOTH) {
        EXTI->FTSR1 |= (1 << pin);  // Enable Falling Edge
    }
    if (trigger == TRIG_RISING || trigger == TRIG_BOTH) {
        EXTI->RTSR1 |= (1 << pin);  // Enable Rising Edge
    }

    // 4. Unmask the designated interrupt line : EXTI - > IMR1
    EXTI->IMR1 |= (1 << pin);

    // Clear any pending flag (PR1, write 1 to clear)
    EXTI->PR1 = (1UL << pin);

    // 5. Enable the shared NVIC line (idempotent if already enabled by another pin)
    NVIC_EnableIRQ(irq_type);
}

void GPIO_OutputTogglePin(GPIO_TypeDef *port, uint8_t pin)
{
    port->ODR ^= (1 << pin);
}

void GPIO_OutputSetPin(GPIO_TypeDef *port, uint8_t pin)
{
    port->ODR |= (1 << pin);
}

void GPIO_OutputClearPin(GPIO_TypeDef *port, uint8_t pin)
{
    port->ODR &= ~(1 << pin);
}

bool GPIO_InputReadPin(GPIO_TypeDef *port, uint8_t pin)
{
    return (port->IDR & (1 << pin)) != 0;  // Check if Pin is high
}
