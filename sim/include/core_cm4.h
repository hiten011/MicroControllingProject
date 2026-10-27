/*
 * core_cm4.h - PC stand-in for the CMSIS Cortex-M4 core header.
 *
 * The real stm32g474xx.h is used unchanged by the simulator build; it
 * includes "core_cm4.h" for the register qualifiers and a few intrinsics.
 * This shim provides just those, so the device header compiles on x86-64.
 * (Only the simulator's include path picks this file up - the ARM build
 * still uses Inc/drivers/CMSIS/Core/Include/core_cm4.h.)
 */
#ifndef SIM_CORE_CM4_H
#define SIM_CORE_CM4_H

#include <stdint.h>

#define __I     volatile const
#define __O     volatile
#define __IO    volatile
#define __IM    volatile const
#define __OM    volatile
#define __IOM   volatile

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif
#ifndef __ASM
#define __ASM __asm__
#endif

#define __NOP()         do { } while (0)
#define __WFI()         do { } while (0)
#define __WFE()         do { } while (0)
#define __DSB()         do { } while (0)
#define __ISB()         do { } while (0)
#define __DMB()         do { } while (0)
#define __enable_irq()  do { } while (0)
#define __disable_irq() do { } while (0)

#endif /* SIM_CORE_CM4_H */
