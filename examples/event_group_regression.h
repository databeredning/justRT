#ifndef JUSTRT_EVENT_GROUP_REGRESSION_H
#define JUSTRT_EVENT_GROUP_REGRESSION_H

#include <stdint.h>

extern volatile uint32_t g_event_regression_wait_any;
extern volatile uint32_t g_event_regression_wait_all;
extern volatile uint32_t g_event_regression_timeouts;
extern volatile uint32_t g_event_regression_isr_sets;
extern volatile uint32_t g_event_regression_isr_wakes;
extern volatile uint32_t g_event_regression_clear_checks;
extern volatile uint32_t g_event_regression_error;
extern volatile uint32_t g_event_regression_done;

void event_group_regression_start(void);

#endif
