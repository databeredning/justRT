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

UNPRIVILEGED_SVC JRT_Status_t JRT_TaskSuspend(uint32_t task_id)
{
    register uint32_t argument asm("r0") = task_id;

    __asm volatile ("svc 4" : "+r" (argument) : : "memory");
    return (JRT_Status_t)argument;
}

UNPRIVILEGED_SVC JRT_Status_t JRT_TaskResume(uint32_t task_id)
{
    register uint32_t argument asm("r0") = task_id;

    __asm volatile ("svc 5" : "+r" (argument) : : "memory");
    return (JRT_Status_t)argument;
}
