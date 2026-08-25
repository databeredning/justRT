#include "kernel.h"
#include "timer.h"
#include "cortex_m/port_contract.h"

#define TIMER_ACTIVE 1U
#define TIMER_INACTIVE 0U

static JRT_Timer_t *timer_list KERNEL_PRIVILEGED_DATA;

static void timer_link(JRT_Timer_t *timer)
{
    JRT_Timer_t *current = timer_list;

    while (current != 0U)
    {
        if (current == timer)
        {
            return;
        }
        current = current->next;
    }

    timer->next = timer_list;
    timer_list = timer;
}

void JRT_TimerCreateStatic(JRT_Timer_t *timer)
{
    if (timer == 0U)
    {
        return;
    }

    timer->deadline = 0U;
    timer->period = 0U;
    timer->reload_ticks = 0U;
    timer->expirations = 0U;
    timer->active = TIMER_INACTIVE;
    timer->periodic = 0U;
    timer->callback = 0U;
    timer->argument = 0U;
    timer->next = 0U;
    timer_link(timer);
}

void JRT_TimerSetCallback(JRT_Timer_t *timer, JRT_TimerCallback_t callback,
                          void *argument)
{
    if (timer != 0U)
    {
        timer->callback = callback;
        timer->argument = argument;
    }
}

void JRT_TimerStart(JRT_Timer_t *timer, uint32_t delay_ticks)
{
    if (timer == 0U)
    {
        return;
    }

    timer_link(timer);
    timer->deadline = JRT_KernelGetTickCount() + delay_ticks;
    timer->period = 0U;
    timer->reload_ticks = delay_ticks;
    timer->periodic = 0U;
    timer->active = TIMER_ACTIVE;
}

void JRT_TimerStartPeriodic(JRT_Timer_t *timer, uint32_t period_ticks)
{
    if (timer == 0U || period_ticks == 0U)
    {
        return;
    }

    timer_link(timer);
    timer->deadline = JRT_KernelGetTickCount() + period_ticks;
    timer->period = period_ticks;
    timer->reload_ticks = period_ticks;
    timer->periodic = 1U;
    timer->active = TIMER_ACTIVE;
}

void JRT_TimerRestart(JRT_Timer_t *timer)
{
    if (timer == 0U || timer->reload_ticks == 0U)
    {
        return;
    }

    timer->deadline = JRT_KernelGetTickCount() + timer->reload_ticks;
    timer->active = TIMER_ACTIVE;
}

void JRT_TimerStop(JRT_Timer_t *timer)
{
    if (timer != 0U)
    {
        timer->active = TIMER_INACTIVE;
    }
}

uint32_t JRT_TimerTakeExpirations(JRT_Timer_t *timer)
{
    uint32_t saved_primask;
    uint32_t expirations;

    if (timer == 0U)
    {
        return 0U;
    }

    saved_primask = arch_critical_enter();
    expirations = timer->expirations;
    timer->expirations = 0U;
    arch_critical_exit(saved_primask);
    return expirations;
}

void JRT_TimerDispatch(JRT_Timer_t *timer)
{
    uint32_t expirations;

    if (timer == 0U || timer->callback == 0U)
    {
        return;
    }

    expirations = JRT_TimerTakeExpirations(timer);
    while (expirations > 0U)
    {
        timer->callback(timer->argument);
        expirations--;
    }
}

void kernel_timer_tick(void)
{
    JRT_Timer_t *timer = timer_list;
    uint32_t now = g_kernel_ticks;

    while (timer != 0U)
    {
        if ((timer->active != 0U)
            && ((int32_t)(now - timer->deadline) >= 0))
        {
            timer->expirations++;
            if (timer->periodic != 0U)
            {
                timer->deadline += timer->period;
            }
            else
            {
                timer->active = TIMER_INACTIVE;
            }
        }
        timer = timer->next;
    }
}
