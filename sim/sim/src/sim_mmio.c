/*
 * sim_mmio.c - put the STM32 peripheral registers at their real addresses.
 *
 * stm32g474xx.h defines e.g. USART3 as ((USART_TypeDef *)0x40004800).
 * We VirtualAlloc() memory at those exact addresses so the student's
 * drivers can be compiled for the PC completely unchanged.
 *
 * RCC and GPIO need no behaviour (plain memory, checked at the end).
 * USART3 and ADC1 do (TXE/RXNE, write-1-to-clear, ADCAL, EOC ...), so
 * their pages are made PAGE_NOACCESS. Every access then faults:
 *
 *   1. access violation -> note register + read/write, let the
 *      peripheral model update the register (before a read), unprotect
 *      the page, set the trap flag and retry the instruction
 *   2. single-step      -> the instruction has completed; tell the model
 *      what was written / read, re-protect the page
 *
 * The firmware just sees registers that behave like hardware.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "sim.h"

#define PAGE            0x1000u
#define TRAP_USART3     0x40004000u     /* USART2/3, UART4/5 page */
#define TRAP_ADC12      0x50000000u     /* ADC1, ADC2, ADC12_COMMON */

static const struct { uintptr_t base; SIZE_T size; } regions[] = {
    { 0x40000000u, 0x30000u },   /* APB1, APB2, AHB1 (RCC @ 0x40021000)    */
    { 0x48000000u, 0x10000u },   /* AHB2: GPIOA..G                         */
    { 0x50000000u, 0x10000u },   /* AHB2: ADC1/2 + common                  */
};

static int trapping;

static struct {
    int       active;
    uintptr_t page;
    uintptr_t reg;
    int       write;
    uint32_t  old_v;
} pend;

static int is_trapped(uintptr_t a)
{
    uintptr_t p = a & ~(uintptr_t)(PAGE - 1u);
    return trapping && (p == TRAP_USART3 || p == TRAP_ADC12);
}

static void protect(uintptr_t page, DWORD prot)
{
    DWORD old;
    VirtualProtect((void *)page, PAGE, prot, &old);
}

static LONG CALLBACK mmio_handler(PEXCEPTION_POINTERS ep)
{
    PEXCEPTION_RECORD er = ep->ExceptionRecord;

    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        er->NumberParameters >= 2 && !pend.active) {
        uintptr_t a = (uintptr_t)er->ExceptionInformation[1];
        if (!is_trapped(a)) return EXCEPTION_CONTINUE_SEARCH;

        pend.active = 1;
        pend.page   = a & ~(uintptr_t)(PAGE - 1u);
        pend.reg    = a & ~(uintptr_t)3u;
        pend.write  = (er->ExceptionInformation[0] == 1);   /* RMW reports as write */
        protect(pend.page, PAGE_READWRITE);
        pend.old_v  = *(volatile uint32_t *)pend.reg;
        if (!pend.write) periph_before_read(pend.reg);

        ep->ContextRecord->EFlags |= 0x100u;                 /* trap flag */
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (er->ExceptionCode == EXCEPTION_SINGLE_STEP && pend.active) {
        uintptr_t reg = pend.reg;
        uintptr_t page = pend.page;
        int       write = pend.write;
        uint32_t  old_v = pend.old_v;

        ep->ContextRecord->EFlags &= ~0x100u;
        pend.active = 0;
        if (write) periph_after_write(reg, old_v, *(volatile uint32_t *)reg);
        else       periph_after_read(reg);
        if (trapping) protect(page, PAGE_NOACCESS);
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int mmio_init(int trap)
{
    for (size_t i = 0; i < sizeof regions / sizeof regions[0]; i++) {
        void *p = VirtualAlloc((void *)regions[i].base, regions[i].size,
                               MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (p != (void *)regions[i].base) {
            fprintf(stderr, "sim: could not map peripheral memory at 0x%08llx (error %lu)\n",
                    (unsigned long long)regions[i].base, GetLastError());
            return 0;
        }
    }
    periph_reset();

    trapping = trap;
    if (trap) {
        AddVectoredExceptionHandler(1, mmio_handler);
        protect(TRAP_USART3, PAGE_NOACCESS);
        protect(TRAP_ADC12, PAGE_NOACCESS);
    }
    return 1;
}

/* Used when the simulator itself needs to read registers outside a fault. */
void mmio_unprotect_all(void)
{
    if (!trapping) return;
    trapping = 0;
    protect(TRAP_USART3, PAGE_READWRITE);
    protect(TRAP_ADC12, PAGE_READWRITE);
}
