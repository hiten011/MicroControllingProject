/*
 * sim_periph.c - register behaviour of the peripherals the drivers use.
 *
 *   USART3  (G474RE_Drivers/uart.c)   the Kobuki link, 115200 8N1
 *   ADC1    (ADC/adc.c)               channel 1 = PA0 = MB1043 analogue out
 *   RCC / GPIO                        plain memory, checked for sane config
 *
 * Simulated time only moves when the firmware is waiting for the Kobuki:
 * a feedback packet is produced (and the world advanced by 20 ms) when the
 * firmware polls USART3->ISR with nothing left to receive. That matches
 * the real program, whose loop is paced by the 50 Hz Kobuki stream.
 */
#define STM32G474xx
#include "stm32g4xx.h"
#include <string.h>
#include "sim.h"

#define ADDR(r) ((uintptr_t)&(r))

/* ---- USART3 ---------------------------------------------------------- */

static uint8_t  rxq[512];
static int      rx_len, rx_pos;
static int      starve;            /* ISR polls in a row with nothing to read */
static int      tx_bytes;          /* bytes sent since the last packet         */
static int      usart_checked;
static char     usart_sum[96] = "USART3 never used";
static int      usart_bad;

/* ---- ADC1 ------------------------------------------------------------ */

int periph_shim;
static int adc_calibrated;
static int adc_checked;
static char adc_sum[96] = "ADC1 never used";
static int  adc_bad;

void periph_reset(void)
{
    memset((void *)RCC, 0, sizeof *RCC);
    RCC->APB1ENR1 = 0x00000400u;

    GPIOA->MODER = 0xABFFFFFFu;  GPIOA->PUPDR = 0x64000000u;
    GPIOB->MODER = 0xFFFFFEBFu;  GPIOB->PUPDR = 0x00000100u;
    GPIOC->MODER = 0xFFFFFFFFu;
    GPIOD->MODER = 0xFFFFFFFFu;

    memset((void *)USART3, 0, sizeof *USART3);
    USART3->ISR = USART_ISR_TXE | USART_ISR_TC;

    memset((void *)ADC1, 0, sizeof *ADC1);
    ADC1->CR = ADC_CR_DEEPPWD;
    ADC12_COMMON->CCR = 0;

    rx_len = rx_pos = starve = tx_bytes = 0;
    usart_checked = adc_checked = adc_calibrated = 0;
}

/* -------------------------------------------------------------------- */
/* Configuration checks: what would stop the real hardware working       */
/* -------------------------------------------------------------------- */

static double usart3_kernel_clock(void)
{
    switch ((RCC->CCIPR & RCC_CCIPR_USART3SEL) >> RCC_CCIPR_USART3SEL_Pos) {
        case 0:  return 16e6;      /* PCLK1 - 16 MHz HSI after reset   */
        case 1:  return 16e6;      /* SYSCLK                           */
        case 2:  return 16e6;      /* HSI16                            */
        default: return 32768.0;   /* LSE                              */
    }
}

static uint32_t gpio_mode(GPIO_TypeDef *g, int pin) { return (g->MODER >> (pin * 2)) & 3u; }
static uint32_t gpio_af(GPIO_TypeDef *g, int pin)   { return (g->AFR[pin / 8] >> ((pin % 8) * 4)) & 0xFu; }

static int check_usart(char *s, size_t n)
{
    int bad = 0;
    double baud = 0.0;

    if (!(RCC->APB1ENR1 & RCC_APB1ENR1_USART3EN)) { sim_warn("u.clk", "USART3 clock not enabled (RCC->APB1ENR1.USART3EN) - its registers would ignore writes"); bad++; }
    if (!(RCC->AHB2ENR & RCC_AHB2ENR_GPIOCEN))    { sim_warn("u.gpio", "GPIOC clock not enabled - PC10/PC11 cannot be configured"); bad++; }
    if (gpio_mode(GPIOC, 10) != 2u || gpio_mode(GPIOC, 11) != 2u) { sim_warn("u.mode", "PC10/PC11 are not in alternate-function mode"); bad++; }
    if (gpio_af(GPIOC, 10) != 7u || gpio_af(GPIOC, 11) != 7u)     { sim_warn("u.af", "PC10/PC11 are not routed to AF7 (USART3)"); bad++; }
    if (!(USART3->CR1 & USART_CR1_UE)) { sim_warn("u.ue", "USART3 not enabled (CR1.UE = 0)"); bad++; }
    if (!(USART3->CR1 & USART_CR1_TE)) { sim_warn("u.te", "USART3 transmitter not enabled (CR1.TE = 0)"); bad++; }
    if (!(USART3->CR1 & USART_CR1_RE)) { sim_warn("u.re", "USART3 receiver not enabled (CR1.RE = 0)"); bad++; }
    if (USART3->CR1 & (USART_CR1_M0 | USART_CR1_M1)) { sim_warn("u.m", "USART3 not set to 8 data bits (Kobuki uses 8N1)"); bad++; }
    if (USART3->CR1 & USART_CR1_PCE) { sim_warn("u.pce", "USART3 parity enabled (Kobuki uses 8N1)"); bad++; }
    if (USART3->CR2 & USART_CR2_STOP) { sim_warn("u.stop", "USART3 not set to 1 stop bit"); bad++; }

    if (USART3->BRR == 0u) {
        sim_warn("u.brr0", "USART3->BRR is 0 - no baud rate set"); bad++;
    } else if (USART3->CR1 & USART_CR1_OVER8) {
        baud = 2.0 * usart3_kernel_clock() / (double)(((USART3->BRR & ~0xFu)) | ((USART3->BRR & 7u) << 1));
    } else {
        baud = usart3_kernel_clock() / (double)USART3->BRR;
    }
    if (baud > 0.0 && (baud < 115200.0 * 0.975 || baud > 115200.0 * 1.025)) {
        sim_warn("u.baud", "USART3 baud rate is %.0f, the Kobuki needs 115200 (+-2.5%%)", baud);
        bad++;
    }
    snprintf(s, n, "USART3 %.0f baud on PC10/PC11", baud);
    return bad;
}

static int check_adc(char *s, size_t n)
{
    int bad = 0;
    if (!(RCC->AHB2ENR & RCC_AHB2ENR_ADC12EN)) { sim_warn("a.clk", "ADC12 clock not enabled (RCC->AHB2ENR.ADC12EN)"); bad++; }
    if (((ADC12_COMMON->CCR & ADC_CCR_CKMODE) == 0u) && ((RCC->CCIPR & RCC_CCIPR_ADC12SEL) == 0u)) {
        sim_warn("a.ck", "ADC12 has no clock: CCR.CKMODE = 0 (async) but RCC->CCIPR.ADC12SEL = 0 (none)"); bad++;
    }
    if (ADC1->CR & ADC_CR_DEEPPWD)        { sim_warn("a.dp", "ADC1 still in deep-power-down (CR.DEEPPWD = 1)"); bad++; }
    if (!(ADC1->CR & ADC_CR_ADVREGEN))     { sim_warn("a.vr", "ADC1 voltage regulator not enabled (CR.ADVREGEN = 0)"); bad++; }
    if (!(ADC1->CR & ADC_CR_ADEN))         { sim_warn("a.en", "ADC1 not enabled (CR.ADEN = 0)"); bad++; }
    if (!adc_calibrated)                   { sim_warn("a.cal", "ADC1 was never calibrated (CR.ADCAL) - readings would be offset"); }
    if (!(RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN)) { sim_warn("a.gpio", "GPIOA clock not enabled (PA0 = ultrasonic input)"); }
    if (gpio_mode(GPIOA, 0) != 3u)         { sim_warn("a.pa0", "PA0 is not in analogue mode - the ultrasonic voltage would not reach ADC1_IN1"); bad++; }
    snprintf(s, n, "ADC1 IN1 (PA0)%s", adc_calibrated ? " calibrated" : "");
    return bad;
}

/* Results are taken when each peripheral is first used (inside its own
 * register trap, so its page is readable) and reported at the end. */
int periph_check_config(char *summary, size_t n)
{
    if (periph_shim) {
        snprintf(summary, n, "shim build - register-level driver checks skipped");
        return 0;
    }
    snprintf(summary, n, "%s; %s", usart_sum, adc_sum);
    return usart_bad + adc_bad;
}

/* -------------------------------------------------------------------- */
/* USART3                                                                */
/* -------------------------------------------------------------------- */

static void usart_load(void)
{
    if (rx_pos < rx_len) {
        USART3->RDR = rxq[rx_pos];
        USART3->ISR |= USART_ISR_RXNE;
    } else {
        USART3->ISR &= ~USART_ISR_RXNE;
    }
}

static void usart_refill(void)
{
    if (!usart_checked) {
        usart_checked = 1;
        usart_bad = check_usart(usart_sum, sizeof usart_sum);
        if (usart_bad && cfg.strict)
            sim_fatal("USART3 is not configured to talk to the Kobuki (see warnings)");
    }
    /* 115200 baud = 86.8 us per byte; one loop has 20 ms of line time.  */
    if ((rx_len + tx_bytes) > 230)
        sim_warn("u.bw", "%d UART bytes in one 20 ms loop - more than 115200 baud can carry", rx_len + tx_bytes);

    sim_feed_needed();
    rx_len   = kobuki_feedback(rxq);
    rx_pos   = 0;
    starve   = 0;
    tx_bytes = 0;
}

/* -------------------------------------------------------------------- */
/* ADC1                                                                  */
/* -------------------------------------------------------------------- */

static void adc_convert(void)
{
    uint32_t ch = (ADC1->SQR1 & ADC_SQR1_SQ1) >> ADC_SQR1_SQ1_Pos;

    if (!adc_checked) {
        adc_checked = 1;
        adc_bad = check_adc(adc_sum, sizeof adc_sum);
        if (adc_bad && cfg.strict)
            sim_fatal("ADC1 is not configured correctly (see warnings)");
    }
    if (!(ADC1->ISR & ADC_ISR_ADRDY))
        sim_warn("a.rdy", "ADC1 conversion started before ADRDY was set");
    if ((ADC1->SQR1 & ADC_SQR1_L) != 0u)
        sim_warn("a.len", "ADC1 sequence length > 1 - only SQ1 is simulated");

    ADC1->DR   = sensor_adc((int)ch);
    ADC1->ISR |= ADC_ISR_EOSMP | ADC_ISR_EOC | ADC_ISR_EOS;
}

static void adc_cr_written(uint32_t old_v, uint32_t cr)
{
    if (cr & ADC_CR_ADCAL) {
        if (cr & ADC_CR_ADEN)
            sim_warn("a.calen", "ADCAL set while the ADC is enabled (not allowed by the reference manual)");
        if ((cr & ADC_CR_DEEPPWD) || !(cr & ADC_CR_ADVREGEN))
            sim_warn("a.calreg", "ADCAL set before the voltage regulator was enabled");
        cr &= ~ADC_CR_ADCAL;                          /* calibration done   */
        adc_calibrated = 1;
    }
    if (cr & ADC_CR_ADDIS) {
        cr &= ~(ADC_CR_ADDIS | ADC_CR_ADEN);
        ADC1->ISR &= ~ADC_ISR_ADRDY;
    }
    if ((cr & ADC_CR_ADEN) && !(old_v & ADC_CR_ADEN))
        ADC1->ISR |= ADC_ISR_ADRDY;                   /* ready at once      */
    if (cr & ADC_CR_ADSTART) {
        adc_convert();
        cr &= ~ADC_CR_ADSTART;                        /* single conversion  */
    }
    ADC1->CR = cr;
}

/* -------------------------------------------------------------------- */
/* Hooks from sim_mmio.c                                                 */
/* -------------------------------------------------------------------- */

void periph_before_read(uintptr_t reg)
{
    if (reg == ADDR(USART3->ISR)) {
        USART3->ISR |= USART_ISR_TXE | USART_ISR_TC;
        if (rx_pos >= rx_len && ++starve >= 2)
            usart_refill();
        usart_load();
    }
}

void periph_after_read(uintptr_t reg)
{
    if (reg == ADDR(USART3->RDR)) {
        if ((USART3->ISR & USART_ISR_RXNE) && rx_pos < rx_len) rx_pos++;
        starve = 0;
        usart_load();
    } else if (reg == ADDR(ADC1->DR)) {
        ADC1->ISR &= ~ADC_ISR_EOC;
    }
}

void periph_after_write(uintptr_t reg, uint32_t old_v, uint32_t new_v)
{
    /* USART3 */
    if (reg == ADDR(USART3->TDR)) {
        kobuki_rx_byte((uint8_t)new_v);
        starve = 0;
        tx_bytes++;
    } else if (reg == ADDR(USART3->ICR)) {
        USART3->ISR &= ~(new_v & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE |
                                  USART_ISR_ORE | USART_ISR_IDLE | USART_ISR_TC));
        USART3->ISR |= USART_ISR_TC;
        USART3->ICR = 0;
    } else if (reg == ADDR(USART3->ISR) || reg == ADDR(USART3->RDR)) {
        *(volatile uint32_t *)reg = old_v;            /* read-only          */
    } else if (reg == ADDR(USART3->CR1)) {
        uint32_t ack = 0;
        if ((new_v & USART_CR1_UE) && (new_v & USART_CR1_TE)) ack |= USART_ISR_TEACK;
        if ((new_v & USART_CR1_UE) && (new_v & USART_CR1_RE)) ack |= USART_ISR_REACK;
        USART3->ISR = (USART3->ISR & ~(USART_ISR_TEACK | USART_ISR_REACK)) | ack;
    }

    /* ADC1 */
    else if (reg == ADDR(ADC1->CR)) {
        adc_cr_written(old_v, new_v);
    } else if (reg == ADDR(ADC1->ISR)) {
        ADC1->ISR = old_v & ~new_v;                   /* write 1 to clear   */
    } else if (reg == ADDR(ADC1->DR)) {
        ADC1->DR = old_v;                             /* read-only          */
    }
}

/* -------------------------------------------------------------------- */
/* Shim build (sim_shim.c): uart.c / adc.c replaced by direct calls so   */
/* the firmware can be run under a debugger. No register checks.        */
/* -------------------------------------------------------------------- */

uint8_t periph_shim_uart_rx(void)
{
    if (rx_pos >= rx_len) {
        usart_checked = 1;
        usart_refill();
    }
    return rxq[rx_pos++];
}

void periph_shim_uart_tx(uint8_t b)
{
    kobuki_rx_byte(b);
    tx_bytes++;
}
