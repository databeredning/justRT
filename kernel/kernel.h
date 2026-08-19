#ifndef JUSTBOOT_KERNEL_H
#define JUSTBOOT_KERNEL_H

#include <stdint.h>

#include "../board/board.h"

void start(void);
void yield(void);
void sleep_ticks(uint32_t ticks);
void led_toggle(void);
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
