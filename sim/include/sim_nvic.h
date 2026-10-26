/* Simulator-only stand-in for the Cortex-M NVIC interrupt enable call.
 * Force-included by sim/build.bat during the host firmware build.
 * Never include this file in the normal ARM/STM32 firmware build.
 */
#ifndef SIM_NVIC_H
#define SIM_NVIC_H
#ifndef NVIC_EnableIRQ
#define NVIC_EnableIRQ(IRQn) ((void)(IRQn))
#endif
#endif
