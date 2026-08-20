#ifndef JUSTBOOT_KERNEL_H
#define JUSTBOOT_KERNEL_H

#include <stdint.h>

#include "sync.h"

#define KERNEL_CORE_CLOCK_HZ 120000000UL
#define KERNEL_TICK_RATE_HZ 7500UL
#define KERNEL_SYSTICK_RELOAD ((KERNEL_CORE_CLOCK_HZ / KERNEL_TICK_RATE_HZ) - 1UL)
#define KERNEL_MAX_TASKS 8U
#define KERNEL_TASK_STACK_WORDS 128U
#define KERNEL_TASK_GUARD_WORDS 8U
#define KERNEL_TASK_STACK_FILL 0xA5A5A5A5UL
#define KERNEL_INITIAL_STACK_USED_WORDS 16U
#define TASK_FLAG_UNPRIVILEGED (1UL << 0)
#define KERNEL_PRIVILEGED __attribute__((section(".privileged_functions")))
#define KERNEL_PRIVILEGED_DATA __attribute__((section(".privileged_data")))
#define TASK_UNPRIVILEGED __attribute__((section(".unprivileged_functions")))
#define TASK_UNPRIVILEGED_DATA __attribute__((section(".unprivileged_task_data")))
#define TASK_UNPRIVILEGED_RODATA __attribute__((section(".unprivileged_rodata")))

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
	KERNEL_ERR_NOT_INITIALIZED,
	KERNEL_ERR_INVALID_TASK
} kernel_status_t;

typedef enum
{
	TASK_STATE_READY = 0U,
	TASK_STATE_RUNNING,
	TASK_STATE_SLEEPING,
	TASK_STATE_BLOCKED
} task_state_t;

typedef struct
{
	uint32_t stack_words;
	uint32_t used_words;
	uint32_t minimum_sp;
	uint32_t current_sp;
} task_stack_info_t;

typedef enum
{
	TASK_WAIT_NONE = 0U,
	TASK_WAIT_SEMAPHORE,
	TASK_WAIT_QUEUE_SEND,
	TASK_WAIT_QUEUE_RECEIVE,
	TASK_WAIT_MUTEX
} task_wait_kind_t;

kernel_status_t kernel_init(const kernel_config_t *config) KERNEL_PRIVILEGED;
void kernel_start(void) KERNEL_PRIVILEGED;
kernel_status_t task_get_state(uint32_t task_id, task_state_t *state) KERNEL_PRIVILEGED;
kernel_status_t task_get_stack_info(uint32_t task_id, task_stack_info_t *info) KERNEL_PRIVILEGED;
kernel_status_t task_get_name(uint32_t task_id, const char **name) KERNEL_PRIVILEGED;
kernel_status_t task_get_priority(uint32_t task_id, uint32_t *priority) KERNEL_PRIVILEGED;
void yield(void);
void sleep_ticks(uint32_t ticks);
uint32_t ms_to_ticks(uint32_t milliseconds);
void tick_init(void) KERNEL_PRIVILEGED;
void request_switch(void) KERNEL_PRIVILEGED;
int kernel_in_isr(void) KERNEL_PRIVILEGED;
uint32_t critical_enter(void) KERNEL_PRIVILEGED;
void critical_exit(uint32_t saved_primask) KERNEL_PRIVILEGED;
void tick_tasks(void) KERNEL_PRIVILEGED;
void sleep_current(uint32_t ticks) KERNEL_PRIVILEGED;
int task_block(void *object, task_wait_kind_t wait_kind, uint32_t timeout_ticks) KERNEL_PRIVILEGED;
void task_wake(void *object, task_wait_kind_t wait_kind) KERNEL_PRIVILEGED;
uint32_t task_current_index(void) KERNEL_PRIVILEGED;
uint32_t task_current_priority(void) KERNEL_PRIVILEGED;
void task_inherit_priority(uint32_t task_id, uint32_t priority) KERNEL_PRIVILEGED;
void task_restore_priority(uint32_t task_id) KERNEL_PRIVILEGED;
uint32_t *pendsv_switch(uint32_t *current_sp) KERNEL_PRIVILEGED;

extern volatile uint32_t g_current_task_index;
extern volatile uint32_t g_idle_kicks;
extern volatile uint32_t g_sync_context_misuse;
extern volatile uint32_t g_sync_misuse_semaphore_take;
extern volatile uint32_t g_sync_misuse_semaphore_give;
extern volatile uint32_t g_sync_misuse_semaphore_give_from_isr;
extern volatile uint32_t g_sync_misuse_mutex_lock;
extern volatile uint32_t g_sync_misuse_mutex_unlock;
extern volatile uint32_t g_sync_misuse_queue_send;
extern volatile uint32_t g_sync_misuse_queue_receive;
extern volatile uint32_t g_sync_misuse_queue_send_from_isr;
extern volatile uint32_t g_context_switches;
extern volatile uint32_t g_ready_scan_depth_max;
extern volatile uint32_t g_sched_pass1_iters_total;
extern volatile uint32_t g_sched_pass2_iters_total;
extern volatile uint32_t g_sched_pass2_iters_max;
extern volatile uint32_t g_isr_queue_send_attempted;
extern volatile uint32_t g_isr_queue_send_accepted;
extern volatile uint32_t g_isr_queue_send_dropped;
extern volatile uint32_t g_isr_queue_count_high_water;
extern volatile uint32_t g_wait_timeout_semaphore;
extern volatile uint32_t g_wait_timeout_queue_send;
extern volatile uint32_t g_wait_timeout_queue_receive;
extern volatile uint32_t g_wait_timeout_mutex;
extern volatile uint32_t g_stack_fault;
extern volatile uint32_t g_stack_fault_task;
extern volatile uint32_t g_stack_fault_sp;

#endif
