#ifndef MY_STM_HELPER_H
#define MY_STM_HELPER_H
#include "stm32c0xx.h"
#include <stdbool.h>

typedef enum { GPIOA_BUS, GPIOB_BUS, GPIOC_BUS, TIM16_BUS, TIM17_BUS, TIM3_BUS } PeripheralBus_t;
typedef enum { INPUT=0b00, OUTPUT=0b01, ALT=0b10, ANALOG=0b11 } GPIOMode_t;
typedef enum { PIN_LOW = 0, PIN_HIGH = 1 } PinState;
typedef enum { NONE=0b00, PULL_UP=0b01, PULL_DOWN=0b10 } GPIOPull_t;
typedef enum { TRIG_RISING = 0, TRIG_FALLING = 1, TRIG_BOTH = 2 } ExtiTrigger;
typedef enum { TIM_CHANNEL_1 = 0, TIM_CHANNEL_2 = 1, TIM_CHANNEL_3 = 2, TIM_CHANNEL_4 = 3 } TimerChannel_t;
typedef enum {
  TIM_MODE_INPUT_CAPTURE, 
  TIM_MODE_OUTPUT_FROZEN, 
  TIM_MODE_OUTPUT_TOGGLE, 
  TIM_MODE_OUTPUT_PWM1,   
  TIM_MODE_OUTPUT_PWM2,   
} TimerChannelMode_t;


void Clock_Enable(PeripheralBus_t peripheral);

// TIMER
void Timer_Init(TIM_TypeDef *timer, IRQn_Type irq_type, uint16_t psc, uint16_t arr, bool enable_interrupt, bool enable_arpe, bool force_update, bool enable_counter);

// PWM
void PWM_Init(TIM_TypeDef *timer, TimerChannel_t channel, TimerChannelMode_t mode, bool enable_counter);

// EXTI
void EXTI_Init(GPIO_TypeDef *port, uint8_t pin, ExtiTrigger trigger, IRQn_Type irq_type);

// GPIO
void GPIO_InitPin(GPIO_TypeDef *port, uint8_t pin, GPIOMode_t mode, GPIOPull_t pull);
void GPIO_InitPins(GPIO_TypeDef *port, uint8_t *pins, GPIOMode_t *mode, GPIOPull_t *pull, uint16_t len);
void GPIO_InitPinsSameMode(GPIO_TypeDef *port, uint8_t *pins, GPIOMode_t mode, GPIOPull_t pull, uint16_t len);
void GPIO_ConnectPeripheral(GPIO_TypeDef *port, uint8_t pin, uint8_t af);

void GPIO_OutputTogglePin(GPIO_TypeDef *port, uint8_t pin);
void GPIO_OutputSetPin(GPIO_TypeDef *port, uint8_t pin);
void GPIO_OutputClearPin(GPIO_TypeDef *port, uint8_t pin);
bool GPIO_InputReadPin(GPIO_TypeDef *port, uint8_t pin);

#endif