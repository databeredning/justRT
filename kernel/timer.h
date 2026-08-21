#ifndef JUSTBOOT_TIMER_H
#define JUSTBOOT_TIMER_H

#include <stdint.h>

typedef void (*kernel_timer_callback_t)(void *argument);

typedef struct kernel_timer
{
    uint32_t deadline;
    uint32_t period;
    uint32_t expirations;
    uint8_t active;
    uint8_t periodic;
    kernel_timer_callback_t callback;
    void *argument;
    struct kernel_timer *next;
} kernel_timer_t;

void kernel_timer_init(kernel_timer_t *timer);
void kernel_timer_start(kernel_timer_t *timer, uint32_t delay_ticks);
void kernel_timer_start_periodic(kernel_timer_t *timer, uint32_t period_ticks);
void kernel_timer_set_callback(kernel_timer_t *timer,
                               kernel_timer_callback_t callback,
                               void *argument);
void kernel_timer_stop(kernel_timer_t *timer);
uint32_t kernel_timer_take_expirations(kernel_timer_t *timer);
void kernel_timer_dispatch(kernel_timer_t *timer);
void kernel_timer_tick(void);

#endif
