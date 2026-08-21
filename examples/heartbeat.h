#ifndef JUSTBOOT_HEARTBEAT_H
#define JUSTBOOT_HEARTBEAT_H

#include <stdint.h>

void heartbeat_example_start(void);
void heartbeat_unprivileged_led_start(void);
void heartbeat_periodic_delay_start(void);

extern volatile uint32_t g_periodic_delay_runs;
extern volatile uint32_t g_periodic_delay_last_tick;

#endif
