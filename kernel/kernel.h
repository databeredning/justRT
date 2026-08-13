#ifndef JUSTBOOT_KERNEL_H
#define JUSTBOOT_KERNEL_H

#include <stdint.h>

void start(void);
void yield(void);
void tick_init(void);
void request_switch(void);
uint32_t *pendsv_switch(uint32_t *current_sp);

extern volatile uint32_t g_yield_count;
extern volatile uint32_t g_boot_counter;
extern volatile uint32_t g_boot_stage;
extern volatile uint32_t g_kernel_started;
extern volatile uint32_t g_current_task_index;
extern volatile uint32_t g_schedule_count;
extern volatile uint32_t g_task0_runs;
extern volatile uint32_t g_task1_runs;
extern volatile uint32_t g_active_task_tag;

#endif
