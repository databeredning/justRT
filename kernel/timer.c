#include "kernel.h"
#include "timer.h"
#include "cortex_m/port_contract.h"

#define TIMER_ACTIVE 1U
#define TIMER_INACTIVE 0U

static JRT_Timer_t *timer_list KERNEL_PRIVILEGED_DATA;

/* Caller holds the kernel critical section. */
static int timer_is_linked_locked(const JRT_Timer_t *timer)
{
    JRT_Timer_t *current = timer_list;

    while (current != 0U)
    {
        if (current == timer)
        {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

/* Caller holds the kernel critical section. */
static void timer_link_locked(JRT_Timer_t *timer)
{
    if (timer_is_linked_locked(timer) == 0)
    {
        timer->next = timer_list;
        timer_list = timer;
    }
}

void JRT_TimerCreateStatic(JRT_Timer_t *timer)
{
    uint32_t saved_primask;
    JRT_Timer_t *next;
    int linked;

    if (timer == 0U)
    {
        return;
    }

    saved_primask = arch_critical_enter();
    linked = timer_is_linked_locked(timer);
    next = (linked != 0) ? timer->next : 0U;
    timer->deadline = 0U;
    timer->period = 0U;
    timer->reload_ticks = 0U;
    timer->expirations = 0U;
    timer->active = TIMER_INACTIVE;
    timer->periodic = 0U;
    timer->callback = 0U;
    timer->argument = 0U;
    timer->next = next;
    timer_link_locked(timer);
    arch_critical_exit(saved_primask);
}

void JRT_TimerSetCallback(JRT_Timer_t *timer, JRT_TimerCallback_t callback,
                          void *argument)
{
    if (timer != 0U)
    {
        uint32_t saved_primask = arch_critical_enter();

        timer->callback = callback;
        timer->argument = argument;
        arch_critical_exit(saved_primask);
    }
}

void JRT_TimerStart(JRT_Timer_t *timer, uint32_t delay_ticks)
{
    uint32_t saved_primask;

    if (timer == 0U)
    {
        return;
    }

    saved_primask = arch_critical_enter();
    timer_link_locked(timer);
    timer->deadline = g_kernel_ticks + delay_ticks;
    timer->period = 0U;
    timer->reload_ticks = delay_ticks;
    timer->periodic = 0U;
    timer->active = TIMER_ACTIVE;
    arch_critical_exit(saved_primask);
}

void JRT_TimerStartPeriodic(JRT_Timer_t *timer, uint32_t period_ticks)
{
    uint32_t saved_primask;

    if (timer == 0U || period_ticks == 0U)
    {
        return;
    }

    saved_primask = arch_critical_enter();
    timer_link_locked(timer);
    timer->deadline = g_kernel_ticks + period_ticks;
    timer->period = period_ticks;
    timer->reload_ticks = period_ticks;
    timer->periodic = 1U;
    timer->active = TIMER_ACTIVE;
    arch_critical_exit(saved_primask);
}

void JRT_TimerRestart(JRT_Timer_t *timer)
{
    uint32_t saved_primask;

    if (timer == 0U)
    {
        return;
    }

    saved_primask = arch_critical_enter();
    if (timer->reload_ticks != 0U)
    {
        timer_link_locked(timer);
        timer->deadline = g_kernel_ticks + timer->reload_ticks;
        timer->active = TIMER_ACTIVE;
    }
    arch_critical_exit(saved_primask);
}

void JRT_TimerStop(JRT_Timer_t *timer)
{
    if (timer != 0U)
    {
        uint32_t saved_primask = arch_critical_enter();

        timer->active = TIMER_INACTIVE;
        arch_critical_exit(saved_primask);
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
    JRT_TimerCallback_t callback;
    void *argument;
    uint32_t expirations;
    uint32_t saved_primask;

    if (timer == 0U)
    {
        return;
    }

    saved_primask = arch_critical_enter();
    callback = timer->callback;
    argument = timer->argument;
    expirations = timer->expirations;
    if (callback != 0U)
    {
        timer->expirations = 0U;
    }
    arch_critical_exit(saved_primask);

    if (callback == 0U)
    {
        return;
    }
    while (expirations > 0U)
    {
        callback(argument);
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
