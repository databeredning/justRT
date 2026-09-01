#include <stdint.h>

#include "kernel.h"
#include "timer.h"
#include "cortex_m/port_contract.h"

#define JRT_IDLE_TASK_COUNT 1U
#define JRT_TIMER_SERVICE_TASK_COUNT 1U
#define JRT_INTERNAL_TASK_COUNT \
    (JRT_IDLE_TASK_COUNT + JRT_TIMER_SERVICE_TASK_COUNT)
#define JRT_MAX_KERNEL_TASKS 3U
#define JRT_MAX_SCHEDULER_TASKS \
    (JRT_MAX_APPLICATION_TASKS + JRT_MAX_KERNEL_TASKS)
#define JRT_TIMER_SERVICE_PRIORITY 1U
#define JRT_TIMER_SERVICE_STACK_WORDS 128U

_Static_assert(JRT_INTERNAL_TASK_COUNT <= JRT_MAX_KERNEL_TASKS,
               "kernel task reservation must cover internal tasks");
_Static_assert((JRT_MAX_APPLICATION_TASKS + JRT_INTERNAL_TASK_COUNT)
               <= JRT_MAX_SCHEDULER_TASKS,
               "scheduler table must include application and internal tasks");
_Static_assert(JRT_TIMER_SERVICE_STACK_WORDS >= JRT_MINIMUM_TASK_STACK_WORDS,
               "timer-service stack is too small");
_Static_assert((JRT_TIMER_SERVICE_STACK_WORDS & 1U) == 0U,
               "timer-service stack must preserve 8-byte alignment");

typedef struct
{
    uint32_t *stack_bottom;
    uint32_t *stack_top;
    uint32_t *sp;
    uint32_t state;
    uint32_t sleep_ticks;
    uint32_t run_count;
    uint32_t *minimum_sp;
    uint32_t high_water_words;
    JRT_TaskEntry_t entry;
    void *argument;
    uint32_t priority;
    uint32_t base_priority;
    const char *name;
    uint32_t flags;
    void *private_data_base;
    uint32_t private_data_size;
    void *wait_object;
    task_wait_kind_t wait_kind;
    uint32_t wait_start;
    uint32_t wait_deadline;
    uint32_t wait_forever;
    uint32_t wait_result;
    uint32_t notification_value;
    uint32_t event_wait_bits;
    uint32_t event_wait_all;
    uint32_t event_clear_on_exit;
} task_t;

typedef struct __attribute__((aligned(32)))
{
    uint32_t guard[JRT_TASK_GUARD_WORDS];
    uint32_t stack[JRT_IDLE_STACK_WORDS];
} idle_task_storage_t;

typedef struct __attribute__((aligned(32)))
{
    uint32_t guard[JRT_TASK_GUARD_WORDS];
    uint32_t stack[JRT_TIMER_SERVICE_STACK_WORDS];
} timer_service_task_storage_t;

volatile uint32_t g_current_task_index KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_idle_kicks KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_context_switches KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_ready_scan_depth_max KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_sched_pass1_iters_total KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_sched_pass2_iters_total KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_sched_pass2_iters_max KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_wait_timeout_semaphore KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_wait_timeout_queue_send KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_wait_timeout_queue_receive KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_wait_timeout_mutex KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_stack_fault KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_stack_fault_task KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_stack_fault_sp KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_kernel_ticks KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_kernel_invariant_active KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_kernel_invariant_code KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_kernel_invariant_task KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_kernel_invariant_object KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_kernel_invariant_aux KERNEL_PRIVILEGED_DATA = 0U;
volatile uint32_t g_kernel_invariant_tick KERNEL_PRIVILEGED_DATA = 0U;
static idle_task_storage_t idle_task_storage JRT_TASK_UNPRIVILEGED_DATA;
static timer_service_task_storage_t timer_service_task_storage
    JRT_TASK_UNPRIVILEGED_DATA;
static uint32_t timer_service_wait_object KERNEL_PRIVILEGED_DATA;
static task_t tasks[JRT_MAX_SCHEDULER_TASKS] KERNEL_PRIVILEGED_DATA = { 0U };
static task_t *current_task KERNEL_PRIVILEGED_DATA = &tasks[0];
static uint32_t task_count KERNEL_PRIVILEGED_DATA;
static uint32_t application_task_count KERNEL_PRIVILEGED_DATA;
static uint32_t kernel_initialized KERNEL_PRIVILEGED_DATA;
static uint32_t kernel_started KERNEL_PRIVILEGED_DATA;
static uint32_t timer_service_task_index KERNEL_PRIVILEGED_DATA;

extern uint8_t __task_private_data_start[];
extern uint8_t __task_private_data_end[];

static void *task_stack_guard(const task_t *task)
{
    return (void *)(task->stack_bottom - JRT_TASK_GUARD_WORDS);
}

static JRT_Status_t validate_task_id(uint32_t task_id)
{
    return (task_id < task_count) ? JRT_STATUS_OK : JRT_STATUS_INVALID_TASK;
}

static JRT_Status_t resolve_application_task_id(uint32_t requested_task_id,
                                                int allow_self,
                                                uint32_t *resolved_task_id)
{
    uint32_t task_id = requested_task_id;

    if ((allow_self != 0) && (task_id == JRT_TASK_ID_SELF))
    {
        task_id = g_current_task_index;
    }
    if (task_id >= application_task_count)
    {
        return JRT_STATUS_INVALID_TASK;
    }
    *resolved_task_id = task_id;
    return JRT_STATUS_OK;
}

JRT_Status_t JRT_TaskGetState(uint32_t task_id, JRT_TaskState_t *state)
{
    uint32_t saved_primask;

    if (state == 0U || validate_task_id(task_id) != JRT_STATUS_OK)
    {
        return JRT_STATUS_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    *state = (JRT_TaskState_t)tasks[task_id].state;
    arch_critical_exit(saved_primask);
    return JRT_STATUS_OK;
}

JRT_Status_t JRT_TaskGetStackInfo(uint32_t task_id, JRT_TaskStackInfo_t *info)
{
    uint32_t saved_primask;
    task_t *task;

    if (info == 0U || validate_task_id(task_id) != JRT_STATUS_OK)
    {
        return JRT_STATUS_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    task = &tasks[task_id];
    info->stack_words = (uint32_t)(task->stack_top - task->stack_bottom);
    info->used_words = task->high_water_words;
    info->minimum_sp = (uint32_t)(uintptr_t)task->minimum_sp;
    info->current_sp = (uint32_t)(uintptr_t)task->sp;
    arch_critical_exit(saved_primask);
    return JRT_STATUS_OK;
}

JRT_Status_t JRT_TaskGetName(uint32_t task_id, const char **name)
{
    uint32_t saved_primask;

    if (name == 0U || validate_task_id(task_id) != JRT_STATUS_OK)
    {
        return JRT_STATUS_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    *name = tasks[task_id].name;
    arch_critical_exit(saved_primask);
    return JRT_STATUS_OK;
}

JRT_Status_t JRT_TaskGetPriority(uint32_t task_id, uint32_t *priority)
{
    uint32_t saved_primask;

    if (priority == 0U || validate_task_id(task_id) != JRT_STATUS_OK)
    {
        return JRT_STATUS_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    *priority = tasks[task_id].priority;
    arch_critical_exit(saved_primask);
    return JRT_STATUS_OK;
}

JRT_Status_t kernel_task_suspend(uint32_t requested_task_id)
{
    uint32_t saved_primask;
    uint32_t task_id;
    JRT_Status_t status;

    saved_primask = arch_critical_enter();
    if (kernel_initialized == 0U)
    {
        arch_critical_exit(saved_primask);
        return JRT_STATUS_NOT_INITIALIZED;
    }
    if (kernel_started == 0U)
    {
        arch_critical_exit(saved_primask);
        return JRT_STATUS_INVALID_CONTEXT;
    }
    status = resolve_application_task_id(requested_task_id, 1, &task_id);
    if (status != JRT_STATUS_OK)
    {
        arch_critical_exit(saved_primask);
        return status;
    }
    if ((tasks[task_id].state != JRT_TASK_STATE_READY)
        && ((tasks[task_id].state != JRT_TASK_STATE_RUNNING)
            || (task_id != g_current_task_index)))
    {
        arch_critical_exit(saved_primask);
        return JRT_STATUS_INVALID_STATE;
    }

    tasks[task_id].state = JRT_TASK_STATE_SUSPENDED;
    arch_critical_exit(saved_primask);
    return JRT_STATUS_OK;
}

JRT_Status_t kernel_task_resume(uint32_t requested_task_id)
{
    uint32_t saved_primask;
    uint32_t task_id;
    JRT_Status_t status;

    saved_primask = arch_critical_enter();
    if (kernel_initialized == 0U)
    {
        arch_critical_exit(saved_primask);
        return JRT_STATUS_NOT_INITIALIZED;
    }
    if (kernel_started == 0U)
    {
        arch_critical_exit(saved_primask);
        return JRT_STATUS_INVALID_CONTEXT;
    }
    status = resolve_application_task_id(requested_task_id, 0, &task_id);
    if (status != JRT_STATUS_OK)
    {
        arch_critical_exit(saved_primask);
        return status;
    }
    if (tasks[task_id].state != JRT_TASK_STATE_SUSPENDED)
    {
        arch_critical_exit(saved_primask);
        return JRT_STATUS_INVALID_STATE;
    }

    tasks[task_id].state = JRT_TASK_STATE_READY;
    arch_critical_exit(saved_primask);
    return JRT_STATUS_OK;
}

uint32_t task_current_index(void)
{
    return g_current_task_index;
}

uint32_t task_current_priority(void)
{
    return current_task->priority;
}

uint32_t *task_current_sp(void)
{
    return current_task->sp;
}

uint32_t task_current_control(void)
{
    return ((current_task->flags & JRT_TASK_FLAG_UNPRIVILEGED) != 0U)
        ? ARCH_LAUNCH_UNPRIVILEGED : ARCH_LAUNCH_PRIVILEGED;
}

void task_inherit_priority(uint32_t task_id, uint32_t priority)
{
    uint32_t saved_primask = arch_critical_enter();
    uint32_t inherited_priority = priority;
    uint32_t owner_id = task_id;
    uint32_t depth;

    for (depth = 0U; depth < task_count; depth++)
    {
        if (owner_id >= task_count)
        {
            break;
        }

        if (tasks[owner_id].priority < inherited_priority)
        {
            tasks[owner_id].priority = inherited_priority;
        }

        if ((tasks[owner_id].state != JRT_TASK_STATE_BLOCKED)
            || (tasks[owner_id].wait_kind != TASK_WAIT_MUTEX)
            || (tasks[owner_id].wait_object == 0U))
        {
            break;
        }

        {
            JRT_Mutex_t *blocking_mutex = (JRT_Mutex_t *)tasks[owner_id].wait_object;

            if ((blocking_mutex->locked == 0U)
                || (blocking_mutex->owner >= task_count)
                || (blocking_mutex->owner == owner_id))
            {
                break;
            }
            owner_id = blocking_mutex->owner;
        }
    }

    arch_critical_exit(saved_primask);
}

/* Recalculate inherited priority up the ownership chain after a waiter is removed. */
static void restore_priority_chain(uint32_t start_id)
{
    uint32_t id = start_id;
    uint32_t depth;

    for (depth = 0U; depth < task_count; depth++)
    {
        uint32_t old_priority;
        uint32_t effective;
        uint32_t scan;

        if (id >= task_count)
        {
            break;
        }

        old_priority = tasks[id].priority;
        effective = tasks[id].base_priority;
        for (scan = 0U; scan < task_count; scan++)
        {
            if ((tasks[scan].state == JRT_TASK_STATE_BLOCKED)
                && (tasks[scan].wait_kind == TASK_WAIT_MUTEX)
                && (tasks[scan].wait_object != 0U))
            {
                JRT_Mutex_t *m = (JRT_Mutex_t *)tasks[scan].wait_object;

                if ((m->locked != 0U)
                    && (m->owner == id)
                    && (tasks[scan].priority > effective))
                {
                    effective = tasks[scan].priority;
                }
            }
        }
        tasks[id].priority = effective;

        if (effective == old_priority)
        {
            break;
        }

        if ((tasks[id].state != JRT_TASK_STATE_BLOCKED)
            || (tasks[id].wait_kind != TASK_WAIT_MUTEX)
            || (tasks[id].wait_object == 0U))
        {
            break;
        }
        {
            JRT_Mutex_t *m = (JRT_Mutex_t *)tasks[id].wait_object;

            if ((m->locked == 0U)
                || (m->owner >= task_count)
                || (m->owner == id))
            {
                break;
            }
            id = m->owner;
        }
    }
}

void task_restore_priority(uint32_t task_id)
{
    uint32_t saved_primask = arch_critical_enter();
    uint32_t index;
    uint32_t effective_priority;

    if (task_id < task_count)
    {
        effective_priority = tasks[task_id].base_priority;

        for (index = 0U; index < task_count; index++)
        {
            if ((tasks[index].state == JRT_TASK_STATE_BLOCKED)
                && (tasks[index].wait_kind == TASK_WAIT_MUTEX)
                && (tasks[index].wait_object != 0U))
            {
                JRT_Mutex_t *mutex = (JRT_Mutex_t *)tasks[index].wait_object;

                if ((mutex->locked != 0U)
                    && (mutex->owner == task_id)
                    && (tasks[index].priority > effective_priority))
                {
                    effective_priority = tasks[index].priority;
                }
            }
        }

        tasks[task_id].priority = effective_priority;
    }
    arch_critical_exit(saved_primask);
}

static void task_exit_trap(void)
{
    while (1)
    {
    }
}

static uint32_t *build_initial_stack(uint32_t *stack_top, JRT_TaskEntry_t entry,
                                     void *argument)
{
    uint32_t *stack = stack_top;

    *--stack = 0x01000000U;
    *--stack = ((uint32_t)entry) & ~1U;
    *--stack = ((uint32_t)task_exit_trap) | 1U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = (uint32_t)(uintptr_t)argument;

    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;

    /* Software-saved context: EXC_RETURN followed by r4-r11. */
    *--stack = ARCH_INITIAL_EXC_RETURN;

    return stack;
}

static void fill_stack(uint32_t *stack_bottom, uint32_t *stack_top)
{
    uint32_t *word;

    for (word = stack_bottom; word < stack_top; word++)
    {
        *word = JRT_TASK_STACK_FILL;
    }
}

static void update_stack_usage(task_t *task, uint32_t *current_sp)
{
    uint32_t *word;

    /*
     * current_sp addresses the software context, including the single-word
     * saved EXC_RETURN.  It is therefore 4-byte aligned even though the PSP
     * recovered after restoring that context is 8-byte aligned.
     */
    if ((current_sp < task->stack_bottom) || (current_sp > task->stack_top)
        || (((uintptr_t)current_sp & 0x3U) != 0U))
    {
        g_stack_fault = 1U;
        g_stack_fault_task = g_current_task_index;
        g_stack_fault_sp = (uint32_t)(uintptr_t)current_sp;
        while (1)
        {
        }
    }

    if (current_sp < task->minimum_sp)
    {
        task->minimum_sp = current_sp;
    }

    word = task->stack_bottom;
    while ((word < task->stack_top) && (*word != JRT_TASK_STACK_FILL))
    {
        word++;
    }
    task->high_water_words = (uint32_t)(task->stack_top - word);
}

/* Shared wait/wake transitions. Callers must hold a critical section. */
static void task_wait_begin(task_t *task, void *object,
                            task_wait_kind_t wait_kind, uint32_t timeout_ticks)
{
    task_wait_deadline_t deadline;

    deadline.start = g_kernel_ticks;
    deadline.tick = deadline.start + timeout_ticks;
    deadline.forever = (timeout_ticks == JRT_WAIT_FOREVER) ? 1U : 0U;
    task->wait_object = object;
    task->wait_kind = wait_kind;
    task->wait_start = deadline.start;
    task->wait_deadline = deadline.tick;
    task->wait_forever = deadline.forever;
    task->wait_result = 0U;
    task->state = JRT_TASK_STATE_BLOCKED;
}

static void task_wait_begin_until(task_t *task, void *object,
                                  task_wait_kind_t wait_kind,
                                  task_wait_deadline_t deadline)
{
    task->wait_object = object;
    task->wait_kind = wait_kind;
    task->wait_start = deadline.start;
    task->wait_deadline = deadline.tick;
    task->wait_forever = deadline.forever;
    task->wait_result = 0U;
    task->state = JRT_TASK_STATE_BLOCKED;
}

static void task_wait_end(task_t *task, uint32_t result)
{
    task->state = JRT_TASK_STATE_READY;
    task->wait_result = result;
    task->wait_object = 0U;
    task->wait_kind = TASK_WAIT_NONE;
    task->wait_start = 0U;
    task->wait_deadline = 0U;
    task->wait_forever = 0U;
}

static void task_wait_reset(task_t *task)
{
    task->wait_object = 0U;
    task->wait_kind = TASK_WAIT_NONE;
    task->wait_start = 0U;
    task->wait_deadline = 0U;
    task->wait_forever = 0U;
    task->wait_result = 0U;
    task->notification_value = 0U;
    task->event_wait_bits = 0U;
    task->event_wait_all = 0U;
    task->event_clear_on_exit = 0U;
}

static void idle_body(void *argument)
{
    (void)argument;
    while (1)
    {
        g_idle_kicks++;
        arch_wait_for_interrupt();
    }
}

static void timer_service_body(void *argument)
{
    JRT_TimerCallback_t callback;
    void *callback_argument;

    (void)argument;
    while (1)
    {
        if (kernel_timer_service_claim(&callback, &callback_argument) != 0)
        {
            callback(callback_argument);
        }
    }
}

static void prepare_task(uint32_t index, const JRT_TaskDefinition_t *definition)
{
    uint32_t *stack_bottom = definition->stack_buffer;
    uint32_t *stack_top = &stack_bottom[definition->stack_words];

    fill_stack(stack_bottom, stack_top);
    tasks[index].stack_bottom = stack_bottom;
    tasks[index].stack_top = stack_top;
    tasks[index].sp = build_initial_stack(stack_top, definition->entry,
                                          definition->argument);
    tasks[index].state = JRT_TASK_STATE_READY;
    tasks[index].minimum_sp = tasks[index].sp;
    tasks[index].high_water_words = JRT_INITIAL_STACK_USED_WORDS;
    tasks[index].entry = definition->entry;
    tasks[index].argument = definition->argument;
    tasks[index].priority = definition->priority;
    tasks[index].base_priority = definition->priority;
    tasks[index].name = definition->name;
    tasks[index].flags = definition->flags;
    tasks[index].private_data_base = definition->private_data_base;
    tasks[index].private_data_size = definition->private_data_size;
    task_wait_reset(&tasks[index]);
}

static void prepare_idle_task(void)
{
    uint32_t idle_index = task_count - 1U;

    fill_stack(&idle_task_storage.stack[0],
               &idle_task_storage.stack[JRT_IDLE_STACK_WORDS]);
    tasks[idle_index].stack_bottom = &idle_task_storage.stack[0];
    tasks[idle_index].stack_top =
        &idle_task_storage.stack[JRT_IDLE_STACK_WORDS];
    tasks[idle_index].sp = build_initial_stack(
        tasks[idle_index].stack_top, idle_body, 0U);
    tasks[idle_index].state = JRT_TASK_STATE_READY;
    tasks[idle_index].minimum_sp = tasks[idle_index].sp;
    tasks[idle_index].high_water_words = JRT_INITIAL_STACK_USED_WORDS;
    tasks[idle_index].entry = idle_body;
    tasks[idle_index].argument = 0U;
    tasks[idle_index].priority = 0U;
    tasks[idle_index].base_priority = 0U;
    tasks[idle_index].name = "idle";
    tasks[idle_index].flags = 0U;
    tasks[idle_index].private_data_base = 0U;
    tasks[idle_index].private_data_size = 0U;
    task_wait_reset(&tasks[idle_index]);
}

static void prepare_timer_service_task(uint32_t index)
{
    task_t *task = &tasks[index];

    timer_service_task_index = index;
    fill_stack(&timer_service_task_storage.stack[0],
               &timer_service_task_storage.stack[JRT_TIMER_SERVICE_STACK_WORDS]);
    task->stack_bottom = &timer_service_task_storage.stack[0];
    task->stack_top =
        &timer_service_task_storage.stack[JRT_TIMER_SERVICE_STACK_WORDS];
    task->sp = build_initial_stack(task->stack_top, timer_service_body, 0U);
    task->minimum_sp = task->sp;
    task->high_water_words = JRT_INITIAL_STACK_USED_WORDS;
    task->entry = timer_service_body;
    task->argument = 0U;
    task->priority = JRT_TIMER_SERVICE_PRIORITY;
    task->base_priority = JRT_TIMER_SERVICE_PRIORITY;
    task->name = "timer-service";
    task->flags = 0U;
    task->private_data_base = 0U;
    task->private_data_size = 0U;
    task_wait_reset(task);
    task_wait_begin(task, &timer_service_wait_object, TASK_WAIT_TIMER_SERVICE,
                    JRT_WAIT_FOREVER);
}

/* Caller holds the kernel critical section. */
void task_timer_service_wake_locked(void)
{
    task_t *task = &tasks[timer_service_task_index];

    if ((task->state == JRT_TASK_STATE_BLOCKED)
        && (task->wait_object == &timer_service_wait_object)
        && (task->wait_kind == TASK_WAIT_TIMER_SERVICE))
    {
        task_wait_end(task, 1U);
    }
}

void task_timer_service_block_locked(uint32_t saved_critical)
{
    task_wait_begin(current_task, &timer_service_wait_object,
                    TASK_WAIT_TIMER_SERVICE, JRT_WAIT_FOREVER);
    arch_critical_exit(saved_critical);
    arch_yield();
}

static int event_condition(uint32_t current, uint32_t requested, uint32_t wait_all)
{
    return (wait_all != 0U) ? ((current & requested) == requested)
                            : ((current & requested) != 0U);
}

void JRT_EventGroupCreateStatic(JRT_EventGroup_t *group)
{
    if (group != 0U)
    {
        group->bits = 0U;
    }
}

static uint32_t event_group_set_bits_common(JRT_EventGroup_t *group,
                                            uint32_t bits, int from_isr)
{
    uint32_t saved_primask;
    uint32_t index;
    uint32_t result;

    if (group == 0U
        || ((from_isr != 0) ? (arch_in_isr() == 0)
                            : (arch_in_isr() != 0)))
    {
        return 0U;
    }
    saved_primask = arch_critical_enter();
    group->bits |= bits;
    result = group->bits;
    for (index = 0U; index < task_count; index++)
    {
        if ((tasks[index].state == JRT_TASK_STATE_BLOCKED)
            && (tasks[index].wait_kind == TASK_WAIT_EVENT_GROUP)
            && (tasks[index].wait_object == group)
            && event_condition(group->bits, tasks[index].event_wait_bits,
                               tasks[index].event_wait_all))
        {
            task_wait_end(&tasks[index], 1U);
        }
    }
    arch_critical_exit(saved_primask);
    arch_request_switch();
    return result;
}

uint32_t JRT_EventGroupSetBits(JRT_EventGroup_t *group, uint32_t bits)
{
    return event_group_set_bits_common(group, bits, 0);
}

uint32_t JRT_EventGroupGetBits(const JRT_EventGroup_t *group)
{
    uint32_t saved_primask;
    uint32_t result;

    if (group == 0U)
    {
        return 0U;
    }
    saved_primask = arch_critical_enter();
    result = group->bits;
    arch_critical_exit(saved_primask);
    return result;
}

uint32_t JRT_EventGroupSetBitsFromISR(JRT_EventGroup_t *group, uint32_t bits)
{
    if (arch_in_isr() == 0)
    {
        return 0U;
    }
    return event_group_set_bits_common(group, bits, 1);
}

uint32_t JRT_EventGroupWaitBits(JRT_EventGroup_t *group, uint32_t bits,
                               int wait_all, int clear_on_exit,
                               uint32_t timeout_ticks)
{
    uint32_t saved_primask;
    uint32_t result;

    if (group == 0U || bits == 0U || arch_in_isr() != 0)
    {
        return 0U;
    }
    saved_primask = arch_critical_enter();
    result = group->bits;
    if (event_condition(result, bits, (wait_all != 0) ? 1U : 0U))
    {
        if (clear_on_exit != 0)
        {
            group->bits &= ~bits;
        }
        arch_critical_exit(saved_primask);
        return result & bits;
    }
    if (timeout_ticks == 0U)
    {
        arch_critical_exit(saved_primask);
        return 0U;
    }
    current_task->event_wait_bits = bits;
    current_task->event_wait_all = (wait_all != 0) ? 1U : 0U;
    current_task->event_clear_on_exit = (clear_on_exit != 0) ? 1U : 0U;
    task_wait_begin(current_task, group, TASK_WAIT_EVENT_GROUP, timeout_ticks);
    arch_critical_exit(saved_primask);
    arch_yield();

    saved_primask = arch_critical_enter();
    result = group->bits & bits;
    if (event_condition(group->bits, bits, current_task->event_wait_all)
        && (current_task->event_clear_on_exit != 0U))
    {
        group->bits &= ~bits;
    }
    arch_critical_exit(saved_primask);
    return result;
}

static int task_notify_common(uint32_t task_id, uint32_t value, int from_isr)
{
    uint32_t saved_primask;

    if ((from_isr != 0) ? (arch_in_isr() == 0) : (arch_in_isr() != 0))
    {
        return 0;
    }
    if (validate_task_id(task_id) != JRT_STATUS_OK)
    {
        return 0;
    }

    saved_primask = arch_critical_enter();
    tasks[task_id].notification_value += value;
    if ((tasks[task_id].state == JRT_TASK_STATE_BLOCKED)
        && (tasks[task_id].wait_kind == TASK_WAIT_NOTIFICATION))
    {
        task_wait_end(&tasks[task_id], 1U);
    }
    arch_critical_exit(saved_primask);
    arch_request_switch();
    return 1;
}

int JRT_TaskNotify(uint32_t task_id, uint32_t value)
{
    return task_notify_common(task_id, value, 0);
}

int JRT_TaskNotifyFromISR(uint32_t task_id, uint32_t value)
{
    return task_notify_common(task_id, value, 1);
}

int JRT_TaskNotifyTake(uint32_t *value, uint32_t timeout_ticks)
{
    uint32_t saved_primask;
    uint32_t notification;

    if (value == 0U || arch_in_isr() != 0)
    {
        return 0;
    }

    saved_primask = arch_critical_enter();
    notification = current_task->notification_value;
    if (notification != 0U)
    {
        current_task->notification_value = 0U;
        arch_critical_exit(saved_primask);
        *value = notification;
        return 1;
    }
    if (timeout_ticks == 0U)
    {
        arch_critical_exit(saved_primask);
        return 0;
    }
    task_wait_begin(current_task, current_task, TASK_WAIT_NOTIFICATION,
                    timeout_ticks);
    arch_critical_exit(saved_primask);
    arch_yield();

    saved_primask = arch_critical_enter();
    notification = current_task->notification_value;
    current_task->notification_value = 0U;
    arch_critical_exit(saved_primask);
    *value = notification;
    return (notification != 0U) ? 1 : 0;
}

void sleep_current(uint32_t ticks)
{
    uint32_t saved_primask = arch_critical_enter();

    current_task->sleep_ticks = ticks;
    current_task->state = (ticks == 0U) ? JRT_TASK_STATE_READY : JRT_TASK_STATE_SLEEPING;
    arch_critical_exit(saved_primask);
}

uint32_t JRT_KernelGetTickCount(void)
{
    uint32_t saved_primask = arch_critical_enter();
    uint32_t ticks = g_kernel_ticks;

    arch_critical_exit(saved_primask);
    return ticks;
}

int JRT_KernelTickReached(uint32_t deadline)
{
    return ((int32_t)(JRT_KernelGetTickCount() - deadline) >= 0) ? 1 : 0;
}

void JRT_TaskDelayUntil(uint32_t *previous_wake, uint32_t period_ticks)
{
    uint32_t next_wake;
    uint32_t now;

    if (previous_wake == 0U)
    {
        return;
    }

    next_wake = *previous_wake + period_ticks;
    *previous_wake = next_wake;
    now = JRT_KernelGetTickCount();
    if ((int32_t)(now - next_wake) < 0)
    {
        JRT_TaskDelay(next_wake - now);
    }
}

int task_block(void *object, task_wait_kind_t wait_kind, uint32_t timeout_ticks)
{
    uint32_t saved_primask;

    if (timeout_ticks == 0U)
    {
        return 0;
    }

    saved_primask = arch_critical_enter();
    task_wait_begin(current_task, object, wait_kind, timeout_ticks);
    arch_critical_exit(saved_primask);
    arch_yield();
    return (int)current_task->wait_result;
}

int task_block_locked(void *object, task_wait_kind_t wait_kind,
                      uint32_t timeout_ticks, uint32_t saved_critical)
{
    task_wait_begin(current_task, object, wait_kind, timeout_ticks);
    arch_critical_exit(saved_critical);
    arch_yield();

    return (int)current_task->wait_result;
}

uint32_t task_wake_get_id(void *object, task_wait_kind_t wait_kind)
{
    uint32_t saved_primask = arch_critical_enter();
    uint32_t index;
    uint32_t selected_index = task_count;
    uint32_t selected_priority = 0U;

    for (index = 0U; index < task_count; index++)
    {
        if ((tasks[index].state == JRT_TASK_STATE_BLOCKED)
            && (tasks[index].wait_object == object)
            && (tasks[index].wait_kind == wait_kind))
        {
            if ((selected_index == task_count)
                || (tasks[index].priority > selected_priority))
            {
                selected_index = index;
                selected_priority = tasks[index].priority;
            }
        }
    }

    if (selected_index != task_count)
    {
        task_wait_end(&tasks[selected_index], 1U);
    }

    arch_critical_exit(saved_primask);
    return (selected_index != task_count) ? selected_index : UINT32_MAX;
}

task_wait_deadline_t task_wait_deadline(uint32_t timeout_ticks)
{
    task_wait_deadline_t deadline;
    uint32_t saved_primask = arch_critical_enter();

    deadline.start = g_kernel_ticks;
    deadline.tick = deadline.start + timeout_ticks;
    deadline.forever = (timeout_ticks == JRT_WAIT_FOREVER) ? 1U : 0U;
    arch_critical_exit(saved_primask);
    return deadline;
}

int task_block_until_locked(void *object, task_wait_kind_t wait_kind,
                            task_wait_deadline_t deadline,
                            uint32_t saved_critical)
{
    if ((deadline.forever == 0U)
        && ((uint32_t)(g_kernel_ticks - deadline.start)
            >= (uint32_t)(deadline.tick - deadline.start)))
    {
        arch_critical_exit(saved_critical);
        return 0;
    }
    task_wait_begin_until(current_task, object, wait_kind, deadline);
    arch_critical_exit(saved_critical);
    arch_yield();

    return (int)current_task->wait_result;
}

int task_wake(void *object, task_wait_kind_t wait_kind)
{
    return (task_wake_get_id(object, wait_kind) != UINT32_MAX) ? 1 : 0;
}

static void kernel_invariant_fail(uint32_t code, uint32_t task_id,
                                  uintptr_t object, uint32_t aux)
{
    g_kernel_invariant_code = code;
    g_kernel_invariant_task = task_id;
    g_kernel_invariant_object = (uint32_t)object;
    g_kernel_invariant_aux = aux;
    g_kernel_invariant_tick = g_kernel_ticks;
    g_kernel_invariant_active = 1U;
    while (1)
    {
    }
}

/* Caller holds the kernel critical section. */
static void kernel_check_invariants_locked(void)
{
    uint32_t index;
    uint32_t code;
    uintptr_t object = 0U;

    for (index = 0U; index < task_count; index++)
    {
        task_t *task = &tasks[index];

        if (task->state > JRT_TASK_STATE_SUSPENDED)
        {
            kernel_invariant_fail(JRT_INVARIANT_TASK_STATE, index,
                                  (uintptr_t)task, task->state);
        }
        if (task->state == JRT_TASK_STATE_BLOCKED)
        {
            if (task->wait_object == 0U)
            {
                kernel_invariant_fail(JRT_INVARIANT_BLOCKED_WAIT_OBJECT,
                                      index, (uintptr_t)task,
                                      (uint32_t)task->wait_kind);
            }
            if ((task->wait_kind == TASK_WAIT_NONE)
                || (task->wait_kind > TASK_WAIT_TIMER_SERVICE))
            {
                kernel_invariant_fail(JRT_INVARIANT_BLOCKED_WAIT_KIND,
                                      index, (uintptr_t)task,
                                      (uint32_t)task->wait_kind);
            }
        }
        else if ((task->wait_object != 0U)
                 || (task->wait_kind != TASK_WAIT_NONE)
                 || (task->wait_start != 0U)
                 || (task->wait_deadline != 0U)
                 || (task->wait_forever != 0U))
        {
            kernel_invariant_fail(JRT_INVARIANT_NONBLOCKED_WAIT_METADATA,
                                  index, (uintptr_t)task,
                                  (uint32_t)task->wait_kind);
        }
    }

    code = sync_invariant_check(task_count, &object);
    if (code != JRT_INVARIANT_NONE)
    {
        kernel_invariant_fail(code, UINT32_MAX, object, 0U);
    }
    code = timer_invariant_check(&object);
    if (code != JRT_INVARIANT_NONE)
    {
        kernel_invariant_fail(code, UINT32_MAX, object, 0U);
    }
}

void tick_tasks(void)
{
    uint32_t saved_primask = arch_critical_enter();
    uint32_t index;

    g_kernel_ticks++;
    kernel_timer_tick();
    for (index = 0U; index < task_count; index++)
    {
        if ((tasks[index].state == JRT_TASK_STATE_SLEEPING) && (tasks[index].sleep_ticks > 0U))
        {
            tasks[index].sleep_ticks--;
            if (tasks[index].sleep_ticks == 0U)
            {
                tasks[index].state = JRT_TASK_STATE_READY;
            }
        }
        else if ((tasks[index].state == JRT_TASK_STATE_BLOCKED)
            && (tasks[index].wait_forever == 0U)
            && ((uint32_t)(g_kernel_ticks - tasks[index].wait_start)
                >= (uint32_t)(tasks[index].wait_deadline
                              - tasks[index].wait_start)))
        {
                uint32_t mutex_owner_id = task_count;
                task_wait_kind_t wait_kind = tasks[index].wait_kind;

                if ((tasks[index].wait_kind == TASK_WAIT_MUTEX)
                    && (tasks[index].wait_object != 0U))
                {
                    JRT_Mutex_t *m = (JRT_Mutex_t *)tasks[index].wait_object;

                    if ((m->locked != 0U) && (m->owner < task_count))
                    {
                        mutex_owner_id = m->owner;
                    }
                }

                task_wait_end(&tasks[index], 0U);

                if (wait_kind == TASK_WAIT_SEMAPHORE)
                {
                    g_wait_timeout_semaphore++;
                }
                else if (wait_kind == TASK_WAIT_QUEUE_SEND)
                {
                    g_wait_timeout_queue_send++;
                }
                else if (wait_kind == TASK_WAIT_QUEUE_RECEIVE)
                {
                    g_wait_timeout_queue_receive++;
                }
                else if (wait_kind == TASK_WAIT_MUTEX)
                {
                    g_wait_timeout_mutex++;
                }

                if (mutex_owner_id < task_count)
                {
                    restore_priority_chain(mutex_owner_id);
                }
        }
    }

    kernel_check_invariants_locked();

    arch_critical_exit(saved_primask);
}

uint32_t *pendsv_switch(uint32_t *current_sp)
{
    uint32_t saved_primask = arch_critical_enter();
    uint32_t offset;
    uint32_t base_index = g_current_task_index;
    uint32_t next_index = base_index;
    uint32_t best_priority = 0U;
    uint32_t selected_offset = task_count;
    uint32_t pass1_iters = 0U;
    uint32_t pass2_iters = 0U;

    current_task->sp = current_sp;
    update_stack_usage(current_task, current_sp);
    current_task->run_count++;
    if (current_task->state == JRT_TASK_STATE_RUNNING)
    {
        current_task->state = JRT_TASK_STATE_READY;
    }

    for (offset = 0U; offset < task_count; offset++)
    {
        pass1_iters++;
        next_index = (base_index + offset) % task_count;
        if ((tasks[next_index].state == JRT_TASK_STATE_READY)
            && (tasks[next_index].priority > best_priority))
        {
            best_priority = tasks[next_index].priority;
        }
    }

    for (offset = 1U; offset <= task_count; offset++)
    {
        pass2_iters++;
        next_index = (base_index + offset) % task_count;
        if ((tasks[next_index].state == JRT_TASK_STATE_READY)
            && (tasks[next_index].priority == best_priority))
        {
            g_current_task_index = next_index;
            selected_offset = offset;
            break;
        }
    }

    g_sched_pass1_iters_total += pass1_iters;
    g_sched_pass2_iters_total += pass2_iters;
    if (pass2_iters > g_sched_pass2_iters_max)
    {
        g_sched_pass2_iters_max = pass2_iters;
    }

    if ((selected_offset < task_count) && (selected_offset > g_ready_scan_depth_max))
    {
        g_ready_scan_depth_max = selected_offset;
    }
    if (g_current_task_index != base_index)
    {
        g_context_switches++;
    }

    current_task = &tasks[g_current_task_index];
    current_task->state = JRT_TASK_STATE_RUNNING;
    arch_set_task_private_data(current_task->private_data_base,
                               current_task->private_data_size);
    arch_set_task_stack_guard(task_stack_guard(current_task));
    arch_critical_exit(saved_primask);
    return current_task->sp;
}

static JRT_Status_t validate_task_stack(const JRT_KernelConfig_t *config,
                                        uint32_t index)
{
    const JRT_TaskDefinition_t *definition = &config->tasks[index];
    uintptr_t guard = (uintptr_t)definition->stack_guard;
    uintptr_t stack = (uintptr_t)definition->stack_buffer;
    uintptr_t stack_bytes;
    uintptr_t end;
    uint32_t previous;

    if ((definition->stack_buffer == 0U) || (definition->stack_guard == 0U)
        || (definition->stack_words < JRT_MINIMUM_TASK_STACK_WORDS)
        || ((definition->stack_words & 1U) != 0U)
        || ((stack & 0x7U) != 0U) || ((guard & 0x1FU) != 0U)
        || ((guard + (JRT_TASK_GUARD_WORDS * sizeof(uint32_t))) != stack)
        || (definition->stack_words > (UINTPTR_MAX / sizeof(uint32_t))))
    {
        return JRT_STATUS_INVALID_STACK;
    }

    stack_bytes = (uintptr_t)definition->stack_words * sizeof(uint32_t);
    end = stack + stack_bytes;
    if (end < stack)
    {
        return JRT_STATUS_INVALID_STACK;
    }

    for (previous = 0U; previous < index; previous++)
    {
        const JRT_TaskDefinition_t *other = &config->tasks[previous];
        uintptr_t other_start = (uintptr_t)other->stack_guard;
        uintptr_t other_end = (uintptr_t)other->stack_buffer
            + ((uintptr_t)other->stack_words * sizeof(uint32_t));

        if ((guard < other_end) && (other_start < end))
        {
            return JRT_STATUS_INVALID_STACK;
        }
    }

    return JRT_STATUS_OK;
}

static int ranges_overlap(uintptr_t first_start, uintptr_t first_end,
                          uintptr_t second_start, uintptr_t second_end)
{
    return ((first_start < second_end) && (second_start < first_end)) ? 1 : 0;
}

static JRT_Status_t validate_private_region(const JRT_KernelConfig_t *config,
                                            uint32_t index)
{
    const JRT_TaskDefinition_t *definition = &config->tasks[index];
    uintptr_t base = (uintptr_t)definition->private_data_base;
    uintptr_t size = (uintptr_t)definition->private_data_size;
    uintptr_t end;
    uint32_t other;

    if ((base == 0U) && (size == 0U))
    {
        return JRT_STATUS_OK;
    }
    if ((base == 0U) || (size < 32U) || ((size & (size - 1U)) != 0U)
        || ((base & (size - 1U)) != 0U)
        || ((definition->flags & JRT_TASK_FLAG_UNPRIVILEGED) == 0U))
    {
        return JRT_STATUS_INVALID_MEMORY_REGION;
    }
    end = base + size;
    if ((end < base)
        || (base < (uintptr_t)__task_private_data_start)
        || (end > (uintptr_t)__task_private_data_end))
    {
        return JRT_STATUS_INVALID_MEMORY_REGION;
    }

    for (other = 0U; other < config->task_count; other++)
    {
        const JRT_TaskDefinition_t *other_definition = &config->tasks[other];
        uintptr_t stack_start = (uintptr_t)other_definition->stack_guard;
        uintptr_t stack_end = (uintptr_t)other_definition->stack_buffer
            + ((uintptr_t)other_definition->stack_words * sizeof(uint32_t));

        if (ranges_overlap(base, end, stack_start, stack_end) != 0)
        {
            return JRT_STATUS_INVALID_MEMORY_REGION;
        }
        if (other < index)
        {
            uintptr_t other_base =
                (uintptr_t)other_definition->private_data_base;
            uintptr_t other_end = other_base
                + (uintptr_t)other_definition->private_data_size;

            if ((other_base != 0U)
                && (ranges_overlap(base, end, other_base, other_end) != 0))
            {
                return JRT_STATUS_INVALID_MEMORY_REGION;
            }
        }
    }
    if (ranges_overlap(base, end,
                       (uintptr_t)&timer_service_task_storage.guard[0],
                       (uintptr_t)&timer_service_task_storage
                           .stack[JRT_TIMER_SERVICE_STACK_WORDS]) != 0
        || ranges_overlap(base, end,
                          (uintptr_t)&idle_task_storage.guard[0],
                          (uintptr_t)&idle_task_storage
                              .stack[JRT_IDLE_STACK_WORDS]) != 0)
    {
        return JRT_STATUS_INVALID_MEMORY_REGION;
    }
    return JRT_STATUS_OK;
}

JRT_Status_t JRT_KernelInit(const JRT_KernelConfig_t *config)
{
    uint32_t configured_total_task_count;
    uint32_t index;

    if (config == 0U || config->tasks == 0U || config->task_count == 0U)
    {
        return JRT_STATUS_INVALID_CONFIG;
    }
    if (config->task_count > JRT_MAX_APPLICATION_TASKS)
    {
        return JRT_STATUS_TOO_MANY_TASKS;
    }
    configured_total_task_count = config->task_count + JRT_INTERNAL_TASK_COUNT;
    if (configured_total_task_count > JRT_MAX_SCHEDULER_TASKS)
    {
        return JRT_STATUS_TOO_MANY_TASKS;
    }
    for (index = 0U; index < config->task_count; index++)
    {
        const JRT_TaskDefinition_t *definition = &config->tasks[index];

        if (definition->entry == 0U)
        {
            return JRT_STATUS_INVALID_ENTRY;
        }
        if (validate_task_stack(config, index) != JRT_STATUS_OK)
        {
            return JRT_STATUS_INVALID_STACK;
        }
    }

    for (index = 0U; index < config->task_count; index++)
    {
        if (validate_private_region(config, index) != JRT_STATUS_OK)
        {
            return JRT_STATUS_INVALID_MEMORY_REGION;
        }
    }

    application_task_count = config->task_count;
    task_count = configured_total_task_count;
    for (index = 0U; index < config->task_count; index++)
    {
        prepare_task(index, &config->tasks[index]);
    }
    prepare_timer_service_task(config->task_count);
    prepare_idle_task();
    current_task = &tasks[0];
    g_current_task_index = 0U;
    g_context_switches = 0U;
    g_ready_scan_depth_max = 0U;
    g_sched_pass1_iters_total = 0U;
    g_sched_pass2_iters_total = 0U;
    g_sched_pass2_iters_max = 0U;
    g_wait_timeout_semaphore = 0U;
    g_wait_timeout_queue_send = 0U;
    g_wait_timeout_queue_receive = 0U;
    g_wait_timeout_mutex = 0U;
    g_kernel_invariant_active = 0U;
    g_kernel_invariant_code = JRT_INVARIANT_NONE;
    g_kernel_invariant_task = 0U;
    g_kernel_invariant_object = 0U;
    g_kernel_invariant_aux = 0U;
    g_kernel_invariant_tick = 0U;
    kernel_started = 0U;
    arch_configure_mpu(task_stack_guard(current_task),
                       current_task->private_data_base,
                       current_task->private_data_size);
    current_task->state = JRT_TASK_STATE_RUNNING;
    kernel_initialized = 1U;
    return JRT_STATUS_OK;
}

void JRT_KernelStart(void)
{
    if (kernel_initialized == 0U)
    {
        while (1)
        {
        }
    }
    arch_tick_init();
    kernel_started = 1U;
    arch_start_first_task();
}
