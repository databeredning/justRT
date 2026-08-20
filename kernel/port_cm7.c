#include <stdint.h>

#include "kernel.h"
#include "../board/board.h"

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
#define CORTEXM_PRIORITY_BITS 4U
#define PENDSV_LOGICAL_PRIORITY 0x0FU
#define SYSTICK_LOGICAL_PRIORITY 0x0EU
#define CORTEXM_PRIORITY_VALUE(priority) \
    ((priority) << (8U - CORTEXM_PRIORITY_BITS))

enum
{
    SVC_SERVICE_YIELD = 0U,
    SVC_SERVICE_SLEEP = 1U
};

void kernel_tick_isr_hook(void) __attribute__((weak));

void kernel_tick_isr_hook(void)
{
}

void tick_init(void)
{
    SCB_SHPR3 = (SCB_SHPR3 & 0x0000FFFFUL)
          | (CORTEXM_PRIORITY_VALUE(PENDSV_LOGICAL_PRIORITY)
              << SCB_SHPR3_PENDSV_SHIFT)
          | (CORTEXM_PRIORITY_VALUE(SYSTICK_LOGICAL_PRIORITY)
              << SCB_SHPR3_SYSTICK_SHIFT);
    SYST_RVR = KERNEL_SYSTICK_RELOAD;
    SYST_CVR = 0UL;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;
}

void request_switch(void)
{
    SCB_ICSR = SCB_ICSR_PENDSVSET;
}

int kernel_in_isr(void)
{
    uint32_t ipsr;

    __asm volatile (
        "mrs %0, ipsr\n"
        : "=r" (ipsr)
        :
        : "memory");

    return (ipsr != 0U) ? 1 : 0;
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

uint32_t ms_to_ticks(uint32_t milliseconds)
{
    uint32_t half_milliseconds = milliseconds >> 1U;
    uint32_t odd_millisecond = milliseconds & 1U;

    return (half_milliseconds * 15U) + (odd_millisecond * 8U);
}

void SysTick_Handler(void)
{
    tick_tasks();
    kernel_tick_isr_hook();
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
        default:
            return;
    }
    request_switch();
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
