#ifndef JUSTBOOT_TIMER_H
#define JUSTBOOT_TIMER_H

#include <stdint.h>

typedef struct kernel_timer
{
    uint32_t deadline;
    uint32_t period;
    uint32_t expirations;
    uint8_t active;
    uint8_t periodic;
    struct kernel_timer *next;
} kernel_timer_t;

void kernel_timer_init(kernel_timer_t *timer);
void kernel_timer_start(kernel_timer_t *timer, uint32_t delay_ticks);
void kernel_timer_start_periodic(kernel_timer_t *timer, uint32_t period_ticks);
void kernel_timer_stop(kernel_timer_t *timer);
uint32_t kernel_timer_take_expirations(kernel_timer_t *timer);
void kernel_timer_tick(void);

#endif
