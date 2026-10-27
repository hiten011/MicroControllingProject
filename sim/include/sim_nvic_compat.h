#ifndef SIM_NVIC_COMPAT_H
#define SIM_NVIC_COMPAT_H
/* PC simulation does not have a real Cortex-M NVIC.
   This allows firmware using NVIC_EnableIRQ() to compile on a PC.
   It does NOT simulate timer interrupts. */
#ifndef NVIC_EnableIRQ
#define NVIC_EnableIRQ(irq) ((void)(irq))
#endif
#endif
