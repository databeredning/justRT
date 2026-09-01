#include <stdint.h>

#include "kernel.h"

volatile uint32_t g_fatal_active KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_fatal_reason KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_fatal_hook_returned KERNEL_PRIVILEGED_DATA = 0U;

void __attribute__((weak)) JRT_FatalErrorHook(JRT_FatalReason_t reason)
{
    (void)reason;
}

void kernel_fatal(JRT_FatalReason_t reason)
{
    __asm volatile ("cpsid i" : : : "memory");
    g_fatal_reason = (uint32_t)reason;
    g_fatal_active = 1U;
    JRT_FatalErrorHook(reason);
    g_fatal_hook_returned = 1U;

    while (1)
    {
        __asm volatile ("nop");
    }
}
