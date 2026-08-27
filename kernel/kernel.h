#ifndef JUSTRT_KERNEL_H
#define JUSTRT_KERNEL_H

#include <stdint.h>

#include "sync.h"

#define JRT_CORE_CLOCK_HZ 120000000UL
#define JRT_TICK_RATE_HZ 7500UL
#define JRT_SYSTICK_RELOAD ((JRT_CORE_CLOCK_HZ / JRT_TICK_RATE_HZ) - 1UL)
#define JRT_MAX_TASKS 8U
#define JRT_TASK_STACK_WORDS 128U
#define JRT_TASK_GUARD_WORDS 8U
#define JRT_TASK_STACK_FILL 0xA5A5A5A5UL
#define JRT_INITIAL_STACK_USED_WORDS 17U
#define JRT_FP_SOFTWARE_CONTEXT_WORDS 16U
#define JRT_FP_HARDWARE_CONTEXT_WORDS 18U
#if JRT_ARCH_FPU_CONTEXT
#define JRT_MINIMUM_TASK_STACK_WORDS \
	(JRT_INITIAL_STACK_USED_WORDS + JRT_FP_SOFTWARE_CONTEXT_WORDS \
	 + JRT_FP_HARDWARE_CONTEXT_WORDS)
#else
#define JRT_MINIMUM_TASK_STACK_WORDS JRT_INITIAL_STACK_USED_WORDS
#endif
#define JRT_TASK_FLAG_UNPRIVILEGED (1UL << 0)
#define KERNEL_PRIVILEGED __attribute__((section(".privileged_functions")))
#define KERNEL_PRIVILEGED_DATA __attribute__((section(".privileged_data")))
#define JRT_TASK_UNPRIVILEGED __attribute__((section(".unprivileged_functions")))
#define JRT_TASK_UNPRIVILEGED_DATA __attribute__((section(".unprivileged_task_data")))
#define JRT_TASK_UNPRIVILEGED_RODATA __attribute__((section(".unprivileged_rodata")))

typedef void (*JRT_TaskEntry_t)(void *argument);

typedef struct
{
	JRT_TaskEntry_t entry;
	void *argument;
	uint32_t stack_words;
	uint32_t priority;
	const char *name;
	uint32_t flags;
} JRT_TaskDefinition_t;

typedef struct
{
	const JRT_TaskDefinition_t *tasks;
	uint32_t task_count;
} JRT_KernelConfig_t;

typedef enum
{
	JRT_STATUS_OK = 0,
	JRT_STATUS_INVALID_CONFIG,
	JRT_STATUS_TOO_MANY_TASKS,
	JRT_STATUS_INVALID_ENTRY,
	JRT_STATUS_INVALID_STACK,
	JRT_STATUS_NOT_INITIALIZED,
	JRT_STATUS_INVALID_TASK
} JRT_Status_t;

typedef enum
{
	JRT_TASK_STATE_READY = 0U,
	JRT_TASK_STATE_RUNNING,
	JRT_TASK_STATE_SLEEPING,
	JRT_TASK_STATE_BLOCKED
} JRT_TaskState_t;

typedef struct
{
	uint32_t stack_words;
	uint32_t used_words;
	uint32_t minimum_sp;
	uint32_t current_sp;
} JRT_TaskStackInfo_t;

typedef enum
{
	TASK_WAIT_NONE = 0U,
	TASK_WAIT_SEMAPHORE,
	TASK_WAIT_QUEUE_SEND,
	TASK_WAIT_QUEUE_RECEIVE,
	TASK_WAIT_MUTEX,
	TASK_WAIT_NOTIFICATION,
	TASK_WAIT_EVENT_GROUP
} task_wait_kind_t;

typedef struct
{
	volatile uint32_t bits;
} JRT_EventGroup_t;

JRT_Status_t JRT_KernelInit(const JRT_KernelConfig_t *config) KERNEL_PRIVILEGED;
void JRT_KernelStart(void) KERNEL_PRIVILEGED;
JRT_Status_t JRT_TaskGetState(uint32_t task_id, JRT_TaskState_t *state) KERNEL_PRIVILEGED;
JRT_Status_t JRT_TaskGetStackInfo(uint32_t task_id, JRT_TaskStackInfo_t *info) KERNEL_PRIVILEGED;
JRT_Status_t JRT_TaskGetName(uint32_t task_id, const char **name) KERNEL_PRIVILEGED;
JRT_Status_t JRT_TaskGetPriority(uint32_t task_id, uint32_t *priority) KERNEL_PRIVILEGED;
void JRT_TaskYield(void);
void JRT_TaskDelay(uint32_t ticks);
void JRT_BoardLedToggle(void);
uint32_t JRT_KernelGetTickCount(void);
int JRT_KernelTickReached(uint32_t deadline);
void JRT_TaskDelayUntil(uint32_t *previous_wake, uint32_t period_ticks);
uint32_t JRT_MillisecondsToTicks(uint32_t milliseconds) JRT_TASK_UNPRIVILEGED;
void tick_init(void) KERNEL_PRIVILEGED;

typedef void (*JRT_KernelTickHook_t)(void);

void JRT_KernelSetTickHook(JRT_KernelTickHook_t hook) KERNEL_PRIVILEGED;
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
uint32_t *task_current_sp(void) KERNEL_PRIVILEGED;
uint32_t task_current_control(void) KERNEL_PRIVILEGED;
void task_inherit_priority(uint32_t task_id, uint32_t priority) KERNEL_PRIVILEGED;
void task_restore_priority(uint32_t task_id) KERNEL_PRIVILEGED;
int JRT_TaskNotify(uint32_t task_id, uint32_t value) KERNEL_PRIVILEGED;
int JRT_TaskNotifyFromISR(uint32_t task_id, uint32_t value) KERNEL_PRIVILEGED;
int JRT_TaskNotifyTake(uint32_t *value, uint32_t timeout_ticks) KERNEL_PRIVILEGED;
void JRT_EventGroupCreateStatic(JRT_EventGroup_t *group) KERNEL_PRIVILEGED;
uint32_t JRT_EventGroupSetBits(JRT_EventGroup_t *group, uint32_t bits) KERNEL_PRIVILEGED;
uint32_t JRT_EventGroupGetBits(const JRT_EventGroup_t *group) KERNEL_PRIVILEGED;
uint32_t JRT_EventGroupSetBitsFromISR(JRT_EventGroup_t *group, uint32_t bits) KERNEL_PRIVILEGED;
uint32_t JRT_EventGroupWaitBits(JRT_EventGroup_t *group, uint32_t bits,
							   int wait_all, int clear_on_exit,
							   uint32_t timeout_ticks) KERNEL_PRIVILEGED;
uint32_t *pendsv_switch(uint32_t *current_sp) KERNEL_PRIVILEGED;
void svc_dispatch(uint32_t *stacked_frame, uint32_t exc_return) KERNEL_PRIVILEGED;

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
extern volatile uint32_t g_kernel_ticks;
extern volatile uint32_t g_svc_invalid_service;
extern volatile uint32_t g_svc_invalid_context;

#endif
