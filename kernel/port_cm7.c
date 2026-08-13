#include <stdint.h>

#include "kernel.h"

#define SYST_CSR (*(volatile uint32_t *)0xE000E010U)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014U)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018U)
#define SCB_ICSR (*(volatile uint32_t *)0xE000ED04U)
#define SCB_SHPR3 (*(volatile uint32_t *)0xE000ED20U)

#define SYST_CSR_ENABLE (1UL << 0)
#define SYST_CSR_TICKINT (1UL << 1)
#define SYST_CSR_CLKSOURCE (1UL << 2)
#define SCB_ICSR_PENDSVSET (1UL << 28)
#define SCB_SHPR3_PENDSV_SHIFT 16U
#define SCB_SHPR3_SYSTICK_SHIFT 24U

volatile uint32_t g_yield_count = 0U;
volatile uint32_t g_tick_count = 0U;
volatile uint32_t g_pendsv_count = 0U;
volatile uint32_t g_systick_armed = 0U;

void tick_init(void)
{
    SCB_SHPR3 = (SCB_SHPR3 & 0x0000FFFFUL)
        | (0xFFUL << SCB_SHPR3_PENDSV_SHIFT)
        | (0xFEUL << SCB_SHPR3_SYSTICK_SHIFT);
    SYST_RVR = 15999UL;
    SYST_CVR = 0UL;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
    g_systick_armed = 1U;
}

void request_switch(void)
{
    SCB_ICSR = SCB_ICSR_PENDSVSET;
}

void yield(void)
{
    __asm volatile ("svc 0" : : : "memory");
}

void sleep_ticks(uint32_t ticks)
{
    register uint32_t argument asm("r0") = ticks;
    __asm volatile ("svc 1" : "+r" (argument) : : "memory");
}

void SysTick_Handler(void)
{
    g_tick_count++;
    tick_tasks();
    request_switch();
}

void svc_dispatch(uint32_t *stacked_frame)
{
    uint8_t svc_number = ((const uint8_t *)stacked_frame[6])[-2];

    if (svc_number == 1U)
    {
        sleep_current(stacked_frame[0]);
    }
    else
    {
        g_yield_count++;
    }

    request_switch();
}

void SVC_Handler(void) __attribute__((naked));

void SVC_Handler(void)
{
    __asm volatile (
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b svc_dispatch\n"
    );
}

void PendSV_Handler(void) __attribute__((naked));

void PendSV_Handler(void)
{
    __asm volatile (
        "mrs r0, psp\n"
        "push {r3, lr}\n"
        "stmdb r0!, {r4-r11}\n"
        "bl pendsv_switch\n"
        "ldmia r0!, {r4-r11}\n"
        "msr psp, r0\n"
        "pop {r3, lr}\n"
        "bx lr\n"
    );
}
