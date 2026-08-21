#ifndef JUSTBOOT_HEARTBEAT_H
#define JUSTBOOT_HEARTBEAT_H

#include <stdint.h>

void heartbeat_example_start(void);
void heartbeat_privilege_counter_start(void);
void heartbeat_unprivileged_svc_start(void);
void heartbeat_unprivileged_led_start(void);
extern volatile uint32_t g_privilege_probe_runs;

#endif
