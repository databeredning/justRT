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

enum
{
    SVC_SERVICE_YIELD = 0U,
    SVC_SERVICE_SLEEP = 1U,
    SVC_SERVICE_LED_TOGGLE = 2U
};

void tick_init(void)
{
    SCB_SHPR3 = (SCB_SHPR3 & 0x0000FFFFUL)
        | (0xFFUL << SCB_SHPR3_PENDSV_SHIFT)
        | (0xFEUL << SCB_SHPR3_SYSTICK_SHIFT);
    SYST_RVR = 15999UL;
    SYST_CVR = 0UL;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
}

void request_switch(void)
{
    SCB_ICSR = SCB_ICSR_PENDSVSET;
}

uint32_t critical_enter(void)
{
    uint32_t saved_primask;

    __asm volatile (
        "mrs %0, primask\n"
        "cpsid i\n"
        : "=r" (saved_primask)
        :
        : "memory");

    return saved_primask;
}

void critical_exit(uint32_t saved_primask)
{
    __asm volatile (
        "msr primask, %0\n"
        :
        : "r" (saved_primask)
        : "memory");
}

void yield(void)
{
    __asm volatile ("svc %c0" : : "I" (SVC_SERVICE_YIELD) : "memory");
}

void sleep_ticks(uint32_t ticks)
{
    register uint32_t argument asm("r0") = ticks;
    __asm volatile ("svc %c1" : "+r" (argument) : "I" (SVC_SERVICE_SLEEP) : "memory");
}

void led_toggle(void)
{
    __asm volatile ("svc %c0" : : "I" (SVC_SERVICE_LED_TOGGLE) : "memory");
}

void SysTick_Handler(void)
{
    tick_tasks();
    request_switch();
}

void svc_dispatch(uint32_t *stacked_frame)
{
    uint8_t svc_number = ((const uint8_t *)stacked_frame[6])[-2];

    switch (svc_number)
    {
        case SVC_SERVICE_YIELD:
            break;
        case SVC_SERVICE_SLEEP:
            sleep_current(stacked_frame[0]);
            break;
        case SVC_SERVICE_LED_TOGGLE:
            board_led_toggle();
            break;
        default:
            return;
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
        "push {r3, lr}\n"
        "bl svc_dispatch\n"
        "pop {r3, lr}\n"
        "bx lr\n"
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
