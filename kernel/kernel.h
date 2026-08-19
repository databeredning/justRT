#ifndef JUSTBOOT_KERNEL_H
#define JUSTBOOT_KERNEL_H

#include <stdint.h>

#include "sync.h"

#define KERNEL_CORE_CLOCK_HZ 120000000UL
#define KERNEL_TICK_RATE_HZ 7500UL
#define KERNEL_SYSTICK_RELOAD ((KERNEL_CORE_CLOCK_HZ / KERNEL_TICK_RATE_HZ) - 1UL)

typedef void (*task_entry_t)(void *argument);

typedef struct
{
	task_entry_t entry;
	void *argument;
	uint32_t stack_words;
	uint32_t priority;
	const char *name;
	uint32_t flags;
} task_definition_t;

typedef struct
{
	const task_definition_t *tasks;
	uint32_t task_count;
} kernel_config_t;

typedef enum
{
	KERNEL_OK = 0,
	KERNEL_ERR_INVALID_CONFIG,
	KERNEL_ERR_TOO_MANY_TASKS,
	KERNEL_ERR_INVALID_ENTRY,
	KERNEL_ERR_INVALID_STACK,
	KERNEL_ERR_NOT_INITIALIZED
} kernel_status_t;

typedef enum
{
	TASK_WAIT_NONE = 0U,
	TASK_WAIT_SEMAPHORE,
	TASK_WAIT_QUEUE_SEND,
	TASK_WAIT_QUEUE_RECEIVE,
	TASK_WAIT_MUTEX
} task_wait_kind_t;

kernel_status_t kernel_init(const kernel_config_t *config);
void kernel_start(void);
void yield(void);
void sleep_ticks(uint32_t ticks);
uint32_t ms_to_ticks(uint32_t milliseconds);
void tick_init(void);
void request_switch(void);
uint32_t critical_enter(void);
void critical_exit(uint32_t saved_primask);
void tick_tasks(void);
void sleep_current(uint32_t ticks);
int task_block(void *object, task_wait_kind_t wait_kind, uint32_t timeout_ticks);
void task_wake(void *object, task_wait_kind_t wait_kind);
uint32_t task_current_index(void);
uint32_t *pendsv_switch(uint32_t *current_sp);

extern volatile uint32_t g_current_task_index;
extern volatile uint32_t g_stack_fault;
extern volatile uint32_t g_stack_fault_task;
extern volatile uint32_t g_stack_fault_sp;

#endif
