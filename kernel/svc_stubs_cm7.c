#include "kernel.h"

#define UNPRIVILEGED_SVC \
    __attribute__((section(".unprivileged_svc"), noinline))

UNPRIVILEGED_SVC void yield(void)
{
    __asm volatile ("svc 1" : : : "memory");
}

UNPRIVILEGED_SVC void sleep_ticks(uint32_t ticks)
{
    register uint32_t argument asm("r0") = ticks;

    __asm volatile ("svc 2" : "+r" (argument) : : "memory");
}

UNPRIVILEGED_SVC void led_toggle(void)
{
    __asm volatile ("svc 3" : : : "memory");
}