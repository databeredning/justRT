#ifndef JUSTRT_KERNEL_H
#define JUSTRT_KERNEL_H

#include <stdint.h>

#include "JRTConfig.h"
#include "sync.h"

#ifndef JRT_ARCH_HAS_DWT_CYCCNT
#define JRT_ARCH_HAS_DWT_CYCCNT 0U
#endif

#if JRT_CORE_CLOCK_HZ == 0UL
#error "JRT_CORE_CLOCK_HZ must be greater than zero"
#endif
#if JRT_CORE_CLOCK_HZ > UINT32_MAX
#error "JRT_CORE_CLOCK_HZ must fit in uint32_t"
#endif
#if JRT_TICK_RATE_HZ == 0UL
#error "JRT_TICK_RATE_HZ must be greater than zero"
#endif
#if JRT_TICK_RATE_HZ > UINT32_MAX
#error "JRT_TICK_RATE_HZ must fit in uint32_t"
#endif
#if JRT_TICK_RATE_HZ > JRT_CORE_CLOCK_HZ
#error "JRT_TICK_RATE_HZ must not exceed JRT_CORE_CLOCK_HZ"
#endif
#if ((JRT_CORE_CLOCK_HZ / JRT_TICK_RATE_HZ) - 1UL) > 0x00FFFFFFUL
#error "Configured SysTick reload exceeds the 24-bit hardware limit"
#endif
#if JRT_MAX_APPLICATION_TASKS == 0U
#error "JRT_MAX_APPLICATION_TASKS must be greater than zero"
#endif
#if JRT_MAX_APPLICATION_TASKS > (UINT32_MAX - 3U)
#error "JRT_MAX_APPLICATION_TASKS exceeds scheduler index capacity"
#endif
#define JRT_SYSTICK_RELOAD ((JRT_CORE_CLOCK_HZ / JRT_TICK_RATE_HZ) - 1UL)
#define JRT_TASK_STACK_WORDS JRT_DEFAULT_TASK_STACK_WORDS
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
#if JRT_DEFAULT_TASK_STACK_WORDS < JRT_MINIMUM_TASK_STACK_WORDS
#error "JRT_DEFAULT_TASK_STACK_WORDS is below the architecture minimum"
#endif
#if (JRT_DEFAULT_TASK_STACK_WORDS & 1U) != 0U
#error "JRT_DEFAULT_TASK_STACK_WORDS must be even for 8-byte alignment"
#endif
#if JRT_DEFAULT_TASK_STACK_WORDS > (UINT32_MAX / 4U)
#error "JRT_DEFAULT_TASK_STACK_WORDS byte size overflows uint32_t"
#endif
#if JRT_IDLE_STACK_WORDS < JRT_MINIMUM_TASK_STACK_WORDS
#error "JRT_IDLE_STACK_WORDS is below the architecture minimum"
#endif
#if (JRT_IDLE_STACK_WORDS & 1U) != 0U
#error "JRT_IDLE_STACK_WORDS must be even for 8-byte alignment"
#endif
#if JRT_IDLE_STACK_WORDS > (UINT32_MAX / 4U)
#error "JRT_IDLE_STACK_WORDS byte size overflows uint32_t"
#endif
#if JRT_TIMER_SERVICE_STACK_WORDS < JRT_MINIMUM_TASK_STACK_WORDS
#error "JRT_TIMER_SERVICE_STACK_WORDS is below the architecture minimum"
#endif
#if (JRT_TIMER_SERVICE_STACK_WORDS & 1U) != 0U
#error "JRT_TIMER_SERVICE_STACK_WORDS must be even for 8-byte alignment"
#endif
#if JRT_TIMER_SERVICE_STACK_WORDS > (UINT32_MAX / 4U)
#error "JRT_TIMER_SERVICE_STACK_WORDS byte size overflows uint32_t"
#endif
#if JRT_MAX_TASK_PRIORITY == 0U
#error "JRT_MAX_TASK_PRIORITY must be greater than zero"
#endif
#if JRT_MAX_TASK_PRIORITY > UINT32_MAX
#error "JRT_MAX_TASK_PRIORITY must fit in uint32_t"
#endif
#if JRT_TIMER_SERVICE_PRIORITY > UINT32_MAX
#error "JRT_TIMER_SERVICE_PRIORITY must fit in uint32_t"
#endif
#if JRT_TIMER_SERVICE_PRIORITY == 0U
#error "JRT_TIMER_SERVICE_PRIORITY must be greater than idle priority"
#endif
#if JRT_TIMER_SERVICE_PRIORITY > JRT_MAX_TASK_PRIORITY
#error "JRT_TIMER_SERVICE_PRIORITY exceeds JRT_MAX_TASK_PRIORITY"
#endif
#if (JRT_ENABLE_TEST_HOOKS != 0U) && (JRT_ENABLE_TEST_HOOKS != 1U)
#error "JRT_ENABLE_TEST_HOOKS must be 0 or 1"
#endif
#if (JRT_ENABLE_TASK_BENCHMARK != 0U) && (JRT_ENABLE_TASK_BENCHMARK != 1U)
#error "JRT_ENABLE_TASK_BENCHMARK must be 0 or 1"
#endif
#if (JRT_ARCH_HAS_DWT_CYCCNT != 0U) && (JRT_ARCH_HAS_DWT_CYCCNT != 1U)
#error "JRT_ARCH_HAS_DWT_CYCCNT must be 0 or 1"
#endif
#if JRT_ENABLE_TASK_BENCHMARK && !JRT_ARCH_HAS_DWT_CYCCNT
#error "JRT_ENABLE_TASK_BENCHMARK requires a supported cycle counter"
#endif
#define JRT_TASK_FLAG_UNPRIVILEGED (1UL << 0)
#define JRT_TASK_ID_SELF UINT32_MAX
#define KERNEL_PRIVILEGED __attribute__((section(".privileged_functions")))
#define KERNEL_PRIVILEGED_DATA __attribute__((section(".privileged_data")))
#define JRT_TASK_UNPRIVILEGED __attribute__((section(".unprivileged_functions")))
#define JRT_TASK_UNPRIVILEGED_DATA __attribute__((section(".unprivileged_task_data")))
#define JRT_TASK_UNPRIVILEGED_RODATA __attribute__((section(".unprivileged_rodata")))
#define JRT_TASK_PRIVATE_DATA(region_size) \
	__attribute__((section(".task_private_data"), aligned(region_size)))

typedef void (*JRT_TaskEntry_t)(void *argument);

#define JRT_DECLARE_STATIC_TASK_STACK(name, word_count)                    \
	static struct __attribute__((aligned(32)))                              \
	{                                                                       \
		uint32_t guard[JRT_TASK_GUARD_WORDS];                                \
		uint32_t words[(word_count)];                                        \
	} name JRT_TASK_UNPRIVILEGED_DATA

#define JRT_TASK_STACK_BUFFER(name) ((name).words)
#define JRT_TASK_STACK_WORD_COUNT(name) \
	((uint32_t)(sizeof((name).words) / sizeof((name).words[0])))
#define JRT_TASK_STACK_GUARD(name) ((void *)&(name).guard[0])
#if JRT_ENABLE_TASK_BENCHMARK
#define JRT_TASK_BENCHMARK_PERIOD_INITIALIZER(period_ticks) \
	.benchmark_period_ticks = (period_ticks),
#else
#define JRT_TASK_BENCHMARK_PERIOD_INITIALIZER(period_ticks)
#endif
#define JRT_TASK_DEFINITION(entry_function, task_argument, stack_name,       \
		task_priority, task_name, task_flags)                                  \
	{                                                                        \
		.entry = (entry_function),                                             \
		.argument = (task_argument),                                           \
		.stack_buffer = JRT_TASK_STACK_BUFFER(stack_name),                     \
		.stack_words = JRT_TASK_STACK_WORD_COUNT(stack_name),                  \
		.stack_guard = JRT_TASK_STACK_GUARD(stack_name),                       \
		.priority = (task_priority),                                           \
		.name = (task_name),                                                   \
		.flags = (task_flags),                                                 \
		.private_data_base = 0U,                                               \
		.private_data_size = 0U,                                               \
		JRT_TASK_BENCHMARK_PERIOD_INITIALIZER(0U)                              \
	}

#define JRT_TASK_DEFINITION_WITH_PERIOD(entry_function, task_argument,      \
		stack_name, task_priority, task_name, task_flags, period_ticks)         \
	{                                                                        \
		.entry = (entry_function),                                             \
		.argument = (task_argument),                                           \
		.stack_buffer = JRT_TASK_STACK_BUFFER(stack_name),                     \
		.stack_words = JRT_TASK_STACK_WORD_COUNT(stack_name),                  \
		.stack_guard = JRT_TASK_STACK_GUARD(stack_name),                       \
		.priority = (task_priority),                                           \
		.name = (task_name),                                                   \
		.flags = (task_flags),                                                 \
		.private_data_base = 0U,                                               \
		.private_data_size = 0U,                                               \
		JRT_TASK_BENCHMARK_PERIOD_INITIALIZER(period_ticks)                    \
	}

#define JRT_TASK_DEFINITION_WITH_PRIVATE_DATA(entry_function, task_argument, \
		stack_name, task_priority, task_name, task_flags, private_object)        \
	{                                                                        \
		.entry = (entry_function),                                             \
		.argument = (task_argument),                                           \
		.stack_buffer = JRT_TASK_STACK_BUFFER(stack_name),                     \
		.stack_words = JRT_TASK_STACK_WORD_COUNT(stack_name),                  \
		.stack_guard = JRT_TASK_STACK_GUARD(stack_name),                       \
		.priority = (task_priority),                                           \
		.name = (task_name),                                                   \
		.flags = (task_flags),                                                 \
		.private_data_base = (void *)&(private_object),                        \
		.private_data_size = (uint32_t)sizeof(private_object),                  \
		JRT_TASK_BENCHMARK_PERIOD_INITIALIZER(0U)                              \
	}

#define JRT_TASK_DEFINITION_WITH_PRIVATE_DATA_AND_PERIOD(                   \
		entry_function, task_argument, stack_name, task_priority, task_name,   \
		task_flags, private_object, period_ticks)                               \
	{                                                                        \
		.entry = (entry_function),                                             \
		.argument = (task_argument),                                           \
		.stack_buffer = JRT_TASK_STACK_BUFFER(stack_name),                     \
		.stack_words = JRT_TASK_STACK_WORD_COUNT(stack_name),                  \
		.stack_guard = JRT_TASK_STACK_GUARD(stack_name),                       \
		.priority = (task_priority),                                           \
		.name = (task_name),                                                   \
		.flags = (task_flags),                                                 \
		.private_data_base = (void *)&(private_object),                        \
		.private_data_size = (uint32_t)sizeof(private_object),                  \
		JRT_TASK_BENCHMARK_PERIOD_INITIALIZER(period_ticks)                    \
	}

typedef struct
{
	JRT_TaskEntry_t entry;
	void *argument;
	uint32_t *stack_buffer;
	uint32_t stack_words;
	void *stack_guard;
	uint32_t priority;
	const char *name;
	uint32_t flags;
	void *private_data_base;
	uint32_t private_data_size;
#if JRT_ENABLE_TASK_BENCHMARK
	uint32_t benchmark_period_ticks;
#endif
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
	JRT_STATUS_INVALID_MEMORY_REGION,
	JRT_STATUS_NOT_INITIALIZED,
	JRT_STATUS_INVALID_TASK,
	JRT_STATUS_INVALID_STATE,
	JRT_STATUS_INVALID_CONTEXT,
	JRT_STATUS_INVALID_PRIORITY
} JRT_Status_t;

typedef enum
{
	JRT_TASK_STATE_READY = 0U,
	JRT_TASK_STATE_RUNNING,
	JRT_TASK_STATE_SLEEPING,
	JRT_TASK_STATE_BLOCKED,
	JRT_TASK_STATE_SUSPENDED
} JRT_TaskState_t;

typedef struct
{
	uint32_t stack_words;
	uint32_t used_words;
	uint32_t minimum_sp;
	uint32_t current_sp;
} JRT_TaskStackInfo_t;

#if JRT_ENABLE_TASK_BENCHMARK
typedef struct
{
	uint32_t release_count;
	uint32_t completion_count;
	uint32_t pending_count;
	uint32_t coalesced_count;
	uint32_t max_release_latency_cycles;
	uint32_t max_activation_cycles;
	uint32_t period_cycles;
	uint32_t stack_words;
	uint32_t used_stack_words;
} JRT_TaskBenchmarkInfo_t;

typedef struct
{
	uint32_t enabled;
	uint32_t task_count;
	uint32_t cycle_frequency_hz;
} JRT_BenchmarkInfo_t;
#endif

typedef enum
{
	TASK_WAIT_NONE = 0U,
	TASK_WAIT_SEMAPHORE,
	TASK_WAIT_QUEUE_SEND,
	TASK_WAIT_QUEUE_RECEIVE,
	TASK_WAIT_MUTEX,
	TASK_WAIT_NOTIFICATION,
	TASK_WAIT_EVENT_GROUP,
	TASK_WAIT_TIMER_SERVICE
} task_wait_kind_t;

typedef struct
{
	uint32_t start;
	uint32_t tick;
	uint32_t forever;
} task_wait_deadline_t;

typedef struct
{
	volatile uint32_t bits;
} JRT_EventGroup_t;

typedef enum
{
	JRT_INVARIANT_NONE = 0U,
	JRT_INVARIANT_TASK_STATE,
	JRT_INVARIANT_BLOCKED_WAIT_OBJECT,
	JRT_INVARIANT_BLOCKED_WAIT_KIND,
	JRT_INVARIANT_NONBLOCKED_WAIT_METADATA,
	JRT_INVARIANT_MUTEX_LIST_CYCLE,
	JRT_INVARIANT_MUTEX_UNLOCKED_STATE,
	JRT_INVARIANT_MUTEX_OWNER,
	JRT_INVARIANT_MUTEX_RECURSION,
	JRT_INVARIANT_TIMER_LIST_CYCLE,
	JRT_INVARIANT_TIMER_PERIODIC_STATE
} JRT_KernelInvariantCode_t;

typedef enum
{
	JRT_FATAL_PROCESSOR_FAULT = 1U,
	JRT_FATAL_KERNEL_INVARIANT,
	JRT_FATAL_STACK_OVERFLOW
} JRT_FatalReason_t;

typedef struct
{
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;
	uint32_t pc;
	uint32_t xpsr;
	uint32_t exc_return;
	uint32_t cfsr;
	uint32_t hfsr;
	uint32_t dfsr;
	uint32_t mmfar;
	uint32_t bfar;
	uint32_t afsr;
	uint32_t fault_type;
} JRT_FaultRecord_t;

JRT_Status_t JRT_KernelInit(const JRT_KernelConfig_t *config) KERNEL_PRIVILEGED;
void JRT_KernelStart(void) KERNEL_PRIVILEGED;
JRT_Status_t JRT_TaskGetState(uint32_t task_id, JRT_TaskState_t *state) KERNEL_PRIVILEGED;
JRT_Status_t JRT_TaskGetStackInfo(uint32_t task_id, JRT_TaskStackInfo_t *info) KERNEL_PRIVILEGED;
JRT_Status_t JRT_TaskGetName(uint32_t task_id, const char **name) KERNEL_PRIVILEGED;
JRT_Status_t JRT_TaskGetPriority(uint32_t task_id, uint32_t *priority) KERNEL_PRIVILEGED;
#if JRT_ENABLE_TASK_BENCHMARK
JRT_Status_t JRT_BenchmarkGetInfo(JRT_BenchmarkInfo_t *info) KERNEL_PRIVILEGED;
JRT_Status_t JRT_BenchmarkGetTask(uint32_t task_id,
	JRT_TaskBenchmarkInfo_t *info) KERNEL_PRIVILEGED;
#endif
JRT_Status_t JRT_TaskSuspend(uint32_t task_id);
JRT_Status_t JRT_TaskResume(uint32_t task_id);
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
void JRT_FatalErrorHook(JRT_FatalReason_t reason);
void kernel_fatal(JRT_FatalReason_t reason) __attribute__((noreturn)) KERNEL_PRIVILEGED;
void request_switch(void) KERNEL_PRIVILEGED;
int kernel_in_isr(void) KERNEL_PRIVILEGED;
uint32_t critical_enter(void) KERNEL_PRIVILEGED;
void critical_exit(uint32_t saved_primask) KERNEL_PRIVILEGED;
void tick_tasks(void) KERNEL_PRIVILEGED;
void sleep_current(uint32_t ticks) KERNEL_PRIVILEGED;
int task_block(void *object, task_wait_kind_t wait_kind, uint32_t timeout_ticks) KERNEL_PRIVILEGED;
int task_block_locked(void *object, task_wait_kind_t wait_kind, uint32_t timeout_ticks, uint32_t saved_critical) KERNEL_PRIVILEGED;
task_wait_deadline_t task_wait_deadline(uint32_t timeout_ticks) KERNEL_PRIVILEGED;
int task_block_until_locked(void *object, task_wait_kind_t wait_kind,
						task_wait_deadline_t deadline,
						uint32_t saved_critical) KERNEL_PRIVILEGED;
int task_wake(void *object, task_wait_kind_t wait_kind) KERNEL_PRIVILEGED;
uint32_t task_wake_get_id(void *object, task_wait_kind_t wait_kind) KERNEL_PRIVILEGED;
uint32_t task_current_index(void) KERNEL_PRIVILEGED;
uint32_t task_current_priority(void) KERNEL_PRIVILEGED;
uint32_t *task_current_sp(void) KERNEL_PRIVILEGED;
uint32_t task_current_control(void) KERNEL_PRIVILEGED;
/* Timer-service transitions; caller holds the kernel critical section. */
void task_timer_service_wake_locked(void) KERNEL_PRIVILEGED;
void task_timer_service_block_locked(uint32_t saved_critical) KERNEL_PRIVILEGED;
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
JRT_Status_t kernel_task_suspend(uint32_t task_id) KERNEL_PRIVILEGED;
JRT_Status_t kernel_task_resume(uint32_t task_id) KERNEL_PRIVILEGED;
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
extern volatile uint32_t g_mpu_stack_guard_base;
extern volatile uint32_t g_mpu_stack_guard_updates;
extern volatile uint32_t g_mpu_private_data_base;
extern volatile uint32_t g_mpu_private_data_size;
extern volatile uint32_t g_mpu_private_data_updates;
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
extern volatile uint32_t g_stack_high_water_words;
extern volatile uint32_t g_stack_high_water_task;
extern volatile uint32_t g_critical_entries;
extern volatile uint32_t g_critical_nesting;
extern volatile uint32_t g_critical_nesting_max;
extern volatile uint32_t g_fatal_active;
extern volatile uint32_t g_fatal_reason;
extern volatile uint32_t g_fatal_hook_returned;
extern volatile JRT_FaultRecord_t g_fault_record;
extern volatile uint32_t g_fault_active;
extern volatile uint32_t g_kernel_ticks;
extern volatile uint32_t g_kernel_invariant_active;
extern volatile uint32_t g_kernel_invariant_code;
extern volatile uint32_t g_kernel_invariant_task;
extern volatile uint32_t g_kernel_invariant_object;
extern volatile uint32_t g_kernel_invariant_aux;
extern volatile uint32_t g_kernel_invariant_tick;
extern volatile uint32_t g_svc_invalid_service;
extern volatile uint32_t g_svc_invalid_context;

#endif
