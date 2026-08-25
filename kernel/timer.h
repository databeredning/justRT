#ifndef JUSTRT_TIMER_H
#define JUSTRT_TIMER_H

#include <stdint.h>

typedef void (*JRT_TimerCallback_t)(void *argument);

typedef struct JRT_Timer
{
    uint32_t deadline;
    uint32_t period;
    uint32_t reload_ticks;
    uint32_t expirations;
    uint8_t active;
    uint8_t periodic;
    JRT_TimerCallback_t callback;
    void *argument;
    struct JRT_Timer *next;
} JRT_Timer_t;

void JRT_TimerCreateStatic(JRT_Timer_t *timer);
void JRT_TimerStart(JRT_Timer_t *timer, uint32_t delay_ticks);
void JRT_TimerStartPeriodic(JRT_Timer_t *timer, uint32_t period_ticks);
void JRT_TimerRestart(JRT_Timer_t *timer);
void JRT_TimerSetCallback(JRT_Timer_t *timer, JRT_TimerCallback_t callback,
                          void *argument);
void JRT_TimerStop(JRT_Timer_t *timer);
uint32_t JRT_TimerTakeExpirations(JRT_Timer_t *timer);
void JRT_TimerDispatch(JRT_Timer_t *timer);
/* Kernel-internal tick processing. */
void kernel_timer_tick(void);

#endif
