#ifndef JUSTBOOT_HEARTBEAT_H
#define JUSTBOOT_HEARTBEAT_H

#include <stdint.h>

void heartbeat_example_start(void);
void heartbeat_unprivileged_led_start(void);
void heartbeat_periodic_delay_start(void);
void heartbeat_timer_start(void);
void heartbeat_timer_callback_start(void);
void heartbeat_notification_start(void);

extern volatile uint32_t g_periodic_delay_runs;
extern volatile uint32_t g_periodic_delay_last_tick;
extern volatile uint32_t g_timer_expirations;
extern volatile uint32_t g_timer_last_tick;
extern volatile uint32_t g_timer_callback_runs;
extern volatile uint32_t g_timer_callback_last_tick;
extern volatile uint32_t g_notification_sent;
extern volatile uint32_t g_notification_received;
extern volatile uint32_t g_notification_error;

#endif
