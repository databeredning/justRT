#include <stdint.h>

#include "kernel.h"
#include "timer.h"
#include "cortex_m/port_contract.h"

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
    task_entry_t entry;
    void *argument;
    uint32_t priority;
    uint32_t base_priority;
    const char *name;
    uint32_t flags;
    void *wait_object;
    task_wait_kind_t wait_kind;
    uint32_t wait_ticks;
    uint32_t wait_result;
    uint32_t notification_value;
    uint32_t event_wait_bits;
    uint32_t event_wait_all;
    uint32_t event_clear_on_exit;
} task_t;

typedef struct __attribute__((aligned(32)))
{
    uint32_t guard[KERNEL_TASK_GUARD_WORDS];
    uint32_t stack[KERNEL_TASK_STACK_WORDS];
} task_storage_t;

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
static task_storage_t task_storage[KERNEL_MAX_TASKS] TASK_UNPRIVILEGED_DATA;
static task_t tasks[KERNEL_MAX_TASKS] KERNEL_PRIVILEGED_DATA = { 0U };
static task_t *current_task KERNEL_PRIVILEGED_DATA = &tasks[0];
static uint32_t task_count KERNEL_PRIVILEGED_DATA;
static uint32_t task_launch_control KERNEL_PRIVILEGED_DATA = 2U;
static uint32_t kernel_initialized KERNEL_PRIVILEGED_DATA;

static kernel_status_t validate_task_id(uint32_t task_id)
{
    return (task_id < task_count) ? KERNEL_OK : KERNEL_ERR_INVALID_TASK;
}

kernel_status_t task_get_state(uint32_t task_id, task_state_t *state)
{
    uint32_t saved_primask;

    if (state == 0U || validate_task_id(task_id) != KERNEL_OK)
    {
        return KERNEL_ERR_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    *state = (task_state_t)tasks[task_id].state;
    arch_critical_exit(saved_primask);
    return KERNEL_OK;
}

kernel_status_t task_get_stack_info(uint32_t task_id, task_stack_info_t *info)
{
    uint32_t saved_primask;
    task_t *task;

    if (info == 0U || validate_task_id(task_id) != KERNEL_OK)
    {
        return KERNEL_ERR_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    task = &tasks[task_id];
    info->stack_words = (uint32_t)(task->stack_top - task->stack_bottom);
    info->used_words = task->high_water_words;
    info->minimum_sp = (uint32_t)(uintptr_t)task->minimum_sp;
    info->current_sp = (uint32_t)(uintptr_t)task->sp;
    arch_critical_exit(saved_primask);
    return KERNEL_OK;
}

kernel_status_t task_get_name(uint32_t task_id, const char **name)
{
    uint32_t saved_primask;

    if (name == 0U || validate_task_id(task_id) != KERNEL_OK)
    {
        return KERNEL_ERR_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    *name = tasks[task_id].name;
    arch_critical_exit(saved_primask);
    return KERNEL_OK;
}

kernel_status_t task_get_priority(uint32_t task_id, uint32_t *priority)
{
    uint32_t saved_primask;

    if (priority == 0U || validate_task_id(task_id) != KERNEL_OK)
    {
        return KERNEL_ERR_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    *priority = tasks[task_id].priority;
    arch_critical_exit(saved_primask);
    return KERNEL_OK;
}

uint32_t task_current_index(void)
{
    return g_current_task_index;
}

uint32_t task_current_priority(void)
{
    return current_task->priority;
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

        if ((tasks[owner_id].state != TASK_STATE_BLOCKED)
            || (tasks[owner_id].wait_kind != TASK_WAIT_MUTEX)
            || (tasks[owner_id].wait_object == 0U))
        {
            break;
        }

        {
            mutex_t *blocking_mutex = (mutex_t *)tasks[owner_id].wait_object;

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
            if ((tasks[scan].state == TASK_STATE_BLOCKED)
                && (tasks[scan].wait_kind == TASK_WAIT_MUTEX)
                && (tasks[scan].wait_object != 0U))
            {
                mutex_t *m = (mutex_t *)tasks[scan].wait_object;

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

        if ((tasks[id].state != TASK_STATE_BLOCKED)
            || (tasks[id].wait_kind != TASK_WAIT_MUTEX)
            || (tasks[id].wait_object == 0U))
        {
            break;
        }
        {
            mutex_t *m = (mutex_t *)tasks[id].wait_object;

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
            if ((tasks[index].state == TASK_STATE_BLOCKED)
                && (tasks[index].wait_kind == TASK_WAIT_MUTEX)
                && (tasks[index].wait_object != 0U))
            {
                mutex_t *mutex = (mutex_t *)tasks[index].wait_object;

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

static uint32_t *build_initial_stack(uint32_t *stack_top, task_entry_t entry,
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

    return stack;
}

static void fill_stack(uint32_t *stack_bottom, uint32_t *stack_top)
{
    uint32_t *word;

    for (word = stack_bottom; word < stack_top; word++)
    {
        *word = KERNEL_TASK_STACK_FILL;
    }
}

static void update_stack_usage(task_t *task, uint32_t *current_sp)
{
    uint32_t *word;

    if ((current_sp < task->stack_bottom) || (current_sp > task->stack_top)
        || (((uintptr_t)current_sp & 0x7U) != 0U))
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
    while ((word < task->stack_top) && (*word != KERNEL_TASK_STACK_FILL))
    {
        word++;
    }
    task->high_water_words = (uint32_t)(task->stack_top - word);
}

/* Shared wait/wake transitions. Callers must hold a critical section. */
static void task_wait_begin(task_t *task, void *object,
                            task_wait_kind_t wait_kind, uint32_t timeout_ticks)
{
    task->wait_object = object;
    task->wait_kind = wait_kind;
    task->wait_ticks = timeout_ticks;
    task->wait_result = 0U;
    task->state = TASK_STATE_BLOCKED;
}

static void task_wait_end(task_t *task, uint32_t result)
{
    task->state = TASK_STATE_READY;
    task->wait_result = result;
    task->wait_object = 0U;
    task->wait_kind = TASK_WAIT_NONE;
    task->wait_ticks = 0U;
}

static void task_wait_reset(task_t *task)
{
    task->wait_object = 0U;
    task->wait_kind = TASK_WAIT_NONE;
    task->wait_ticks = 0U;
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
        __asm volatile ("wfi" : : : "memory");
    }
}

static void prepare_task(uint32_t index, const task_definition_t *definition)
{
    task_storage_t *storage = &task_storage[index];
    uint32_t *stack_bottom = &storage->stack[0];
    uint32_t *stack_top = &storage->stack[definition->stack_words];

    fill_stack(stack_bottom, stack_top);
    tasks[index].stack_bottom = stack_bottom;
    tasks[index].stack_top = stack_top;
    tasks[index].sp = build_initial_stack(stack_top, definition->entry,
                                          definition->argument);
    tasks[index].state = TASK_STATE_READY;
    tasks[index].minimum_sp = tasks[index].sp;
    tasks[index].high_water_words = KERNEL_INITIAL_STACK_USED_WORDS;
    tasks[index].entry = definition->entry;
    tasks[index].argument = definition->argument;
    tasks[index].priority = definition->priority;
    tasks[index].base_priority = definition->priority;
    tasks[index].name = definition->name;
    tasks[index].flags = definition->flags;
    task_wait_reset(&tasks[index]);
}

static void prepare_idle_task(void)
{
    uint32_t idle_index = task_count - 1U;

    fill_stack(&task_storage[idle_index].stack[0],
               &task_storage[idle_index].stack[KERNEL_TASK_STACK_WORDS]);
    tasks[idle_index].stack_bottom = &task_storage[idle_index].stack[0];
    tasks[idle_index].stack_top = &task_storage[idle_index].stack[KERNEL_TASK_STACK_WORDS];
    tasks[idle_index].sp = build_initial_stack(
        tasks[idle_index].stack_top, idle_body, 0U);
    tasks[idle_index].state = TASK_STATE_READY;
    tasks[idle_index].minimum_sp = tasks[idle_index].sp;
    tasks[idle_index].high_water_words = KERNEL_INITIAL_STACK_USED_WORDS;
    tasks[idle_index].entry = idle_body;
    tasks[idle_index].argument = 0U;
    tasks[idle_index].priority = 0U;
    tasks[idle_index].base_priority = 0U;
    tasks[idle_index].name = "idle";
    tasks[idle_index].flags = 0U;
    task_wait_reset(&tasks[idle_index]);
}

static int event_condition(uint32_t current, uint32_t requested, uint32_t wait_all)
{
    return (wait_all != 0U) ? ((current & requested) == requested)
                            : ((current & requested) != 0U);
}

void event_group_init(event_group_t *group)
{
    if (group != 0U)
    {
        group->bits = 0U;
    }
}

static uint32_t event_group_set_bits_common(event_group_t *group,
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
        if ((tasks[index].state == TASK_STATE_BLOCKED)
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

uint32_t event_group_set_bits(event_group_t *group, uint32_t bits)
{
    return event_group_set_bits_common(group, bits, 0);
}

uint32_t event_group_get_bits(const event_group_t *group)
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

uint32_t event_group_set_bits_from_isr(event_group_t *group, uint32_t bits)
{
    if (arch_in_isr() == 0)
    {
        return 0U;
    }
    return event_group_set_bits_common(group, bits, 1);
}

uint32_t event_group_wait_bits(event_group_t *group, uint32_t bits,
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
    if (validate_task_id(task_id) != KERNEL_OK)
    {
        return 0;
    }

    saved_primask = arch_critical_enter();
    tasks[task_id].notification_value += value;
    if ((tasks[task_id].state == TASK_STATE_BLOCKED)
        && (tasks[task_id].wait_kind == TASK_WAIT_NOTIFICATION))
    {
        task_wait_end(&tasks[task_id], 1U);
    }
    arch_critical_exit(saved_primask);
    arch_request_switch();
    return 1;
}

int task_notify(uint32_t task_id, uint32_t value)
{
    return task_notify_common(task_id, value, 0);
}

int task_notify_from_isr(uint32_t task_id, uint32_t value)
{
    return task_notify_common(task_id, value, 1);
}

int task_notify_take(uint32_t *value, uint32_t timeout_ticks)
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
    current_task->state = (ticks == 0U) ? TASK_STATE_READY : TASK_STATE_SLEEPING;
    arch_critical_exit(saved_primask);
}

uint32_t kernel_ticks_now(void)
{
    uint32_t saved_primask = arch_critical_enter();
    uint32_t ticks = g_kernel_ticks;

    arch_critical_exit(saved_primask);
    return ticks;
}

int kernel_tick_reached(uint32_t deadline)
{
    return ((int32_t)(kernel_ticks_now() - deadline) >= 0) ? 1 : 0;
}

void task_delay_until(uint32_t *previous_wake, uint32_t period_ticks)
{
    uint32_t next_wake;
    uint32_t now;

    if (previous_wake == 0U)
    {
        return;
    }

    next_wake = *previous_wake + period_ticks;
    *previous_wake = next_wake;
    now = kernel_ticks_now();
    if ((int32_t)(now - next_wake) < 0)
    {
        sleep_ticks(next_wake - now);
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

void task_wake(void *object, task_wait_kind_t wait_kind)
{
    uint32_t saved_primask = arch_critical_enter();
    uint32_t index;
    uint32_t selected_index = task_count;
    uint32_t selected_priority = 0U;

    for (index = 0U; index < task_count; index++)
    {
        if ((tasks[index].state == TASK_STATE_BLOCKED)
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
}

void tick_tasks(void)
{
    uint32_t saved_primask = arch_critical_enter();
    uint32_t index;

    g_kernel_ticks++;
    kernel_timer_tick();
    for (index = 0U; index < task_count; index++)
    {
        if ((tasks[index].state == TASK_STATE_SLEEPING) && (tasks[index].sleep_ticks > 0U))
        {
            tasks[index].sleep_ticks--;
            if (tasks[index].sleep_ticks == 0U)
            {
                tasks[index].state = TASK_STATE_READY;
            }
        }
        else if ((tasks[index].state == TASK_STATE_BLOCKED)
            && (tasks[index].wait_ticks != SEMAPHORE_WAIT_FOREVER)
            && (tasks[index].wait_ticks > 0U))
        {
            tasks[index].wait_ticks--;
            if (tasks[index].wait_ticks == 0U)
            {
                uint32_t mutex_owner_id = task_count;
                task_wait_kind_t wait_kind = tasks[index].wait_kind;

                if ((tasks[index].wait_kind == TASK_WAIT_MUTEX)
                    && (tasks[index].wait_object != 0U))
                {
                    mutex_t *m = (mutex_t *)tasks[index].wait_object;

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
    }

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
    if (current_task->state == TASK_STATE_RUNNING)
    {
        current_task->state = TASK_STATE_READY;
    }

    for (offset = 0U; offset < task_count; offset++)
    {
        pass1_iters++;
        next_index = (base_index + offset) % task_count;
        if ((tasks[next_index].state == TASK_STATE_READY)
            && (tasks[next_index].priority > best_priority))
        {
            best_priority = tasks[next_index].priority;
        }
    }

    for (offset = 1U; offset <= task_count; offset++)
    {
        pass2_iters++;
        next_index = (base_index + offset) % task_count;
        if ((tasks[next_index].state == TASK_STATE_READY)
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
    current_task->state = TASK_STATE_RUNNING;
    arch_critical_exit(saved_primask);
    return current_task->sp;
}

static void launch_first_task(uint32_t *sp __attribute__((unused))) __attribute__((naked));

static void launch_first_task(uint32_t *sp __attribute__((unused)))
{
    __asm volatile (
        "ldmia   r0!, {r4-r11}          \n"
        "ldr     lr,  [r0, #20]         \n"
        "ldr     r2,  [r0, #24]         \n"
        "orr     r2,  r2, #1            \n"
        "adds    r0,  r0, #32           \n"
        "msr     psp, r0                \n"
        "cpsie   i                      \n"
        "ldr     r1,  =task_launch_control\n"
        "ldr     r0,  [r1]              \n"
        "msr     control, r0            \n"
        "isb                            \n"
        "movs    r0,  #0                \n"
        "movs    r1,  #0                \n"
        "movs    r3,  #0                \n"
        "bx      r2                     \n"
    );
}

kernel_status_t kernel_init(const kernel_config_t *config)
{
    uint32_t index;

    if (config == 0U || config->tasks == 0U || config->task_count == 0U)
    {
        return KERNEL_ERR_INVALID_CONFIG;
    }
    if ((config->task_count + 1U) > KERNEL_MAX_TASKS)
    {
        return KERNEL_ERR_TOO_MANY_TASKS;
    }
    if ((config->task_count + 1U) > ARCH_MPU_GUARD_REGION_COUNT)
    {
        return KERNEL_ERR_TOO_MANY_TASKS;
    }
    for (index = 0U; index < config->task_count; index++)
    {
        const task_definition_t *definition = &config->tasks[index];

        if (definition->entry == 0U)
        {
            return KERNEL_ERR_INVALID_ENTRY;
        }
        if ((definition->stack_words == 0U)
            || (definition->stack_words > KERNEL_TASK_STACK_WORDS)
            || ((definition->stack_words & 1U) != 0U))
        {
            return KERNEL_ERR_INVALID_STACK;
        }
    }

    task_count = config->task_count + 1U;
    for (index = 0U; index < config->task_count; index++)
    {
        prepare_task(index, &config->tasks[index]);
    }
    prepare_idle_task();
    {
        void *guard_addresses[KERNEL_MAX_TASKS];
        uint32_t guard_index;

        for (guard_index = 0U; guard_index < task_count; guard_index++)
        {
            guard_addresses[guard_index] = &task_storage[guard_index].guard[0];
        }
        arch_configure_mpu(guard_addresses, task_count);
    }
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
    current_task->state = TASK_STATE_RUNNING;
    kernel_initialized = 1U;
    return KERNEL_OK;
}

void kernel_start(void)
{
    if (kernel_initialized == 0U)
    {
        while (1)
        {
        }
    }
    arch_tick_init();
    task_launch_control = ((current_task->flags & TASK_FLAG_UNPRIVILEGED) != 0U)
        ? 3U : 2U;
    launch_first_task(current_task->sp);
}
