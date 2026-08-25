#include "kernel.h"

#define UNPRIVILEGED_SVC \
    __attribute__((section(".unprivileged_svc"), noinline))

UNPRIVILEGED_SVC void JRT_TaskYield(void)
{
    __asm volatile ("svc 1" : : : "memory");
}

UNPRIVILEGED_SVC void JRT_TaskDelay(uint32_t ticks)
{
    register uint32_t argument asm("r0") = ticks;

    __asm volatile ("svc 2" : "+r" (argument) : : "memory");
}

UNPRIVILEGED_SVC void JRT_BoardLedToggle(void)
{
    __asm volatile ("svc 3" : : : "memory");
}
