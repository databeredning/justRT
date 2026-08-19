#ifndef JUSTBOOT_KERNEL_H
#define JUSTBOOT_KERNEL_H

#include <stdint.h>

#include "sync.h"

#define KERNEL_CORE_CLOCK_HZ 120000000UL
#define KERNEL_TICK_RATE_HZ 7500UL
#define KERNEL_SYSTICK_RELOAD ((KERNEL_CORE_CLOCK_HZ / KERNEL_TICK_RATE_HZ) - 1UL)

typedef void (*task_entry_t)(void);

typedef struct
{
	const task_entry_t *entries;
	uint32_t count;
} task_config_t;

void kernel_start(const task_config_t *config);
void yield(void);
void sleep_ticks(uint32_t ticks);
uint32_t ms_to_ticks(uint32_t milliseconds);
void tick_init(void);
void request_switch(void);
uint32_t critical_enter(void);
void critical_exit(uint32_t saved_primask);
void tick_tasks(void);
void sleep_current(uint32_t ticks);
uint32_t *pendsv_switch(uint32_t *current_sp);

extern volatile uint32_t g_current_task_index;
extern volatile uint32_t g_stack_fault;
extern volatile uint32_t g_stack_fault_task;
extern volatile uint32_t g_stack_fault_sp;

#endif
