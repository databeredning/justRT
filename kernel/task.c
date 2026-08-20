#include <stdint.h>

#include "kernel.h"

#define MPU_CTRL (*(volatile uint32_t *)0xE000ED94U)
#define MPU_RNR (*(volatile uint32_t *)0xE000ED98U)
#define MPU_RBAR (*(volatile uint32_t *)0xE000ED9CU)
#define MPU_RASR (*(volatile uint32_t *)0xE000EDA0U)
#define SCB_SHCSR (*(volatile uint32_t *)0xE000ED24U)

#define MPU_CTRL_ENABLE (1UL << 0)
#define MPU_CTRL_PRIVDEFENA (1UL << 2)
#define MPU_RASR_ENABLE (1UL << 0)
#define MPU_RASR_XN (1UL << 28)
#define MPU_RASR_SIZE_32_BYTES (4UL << 1)
#define SCB_SHCSR_MEMFAULTENA (1UL << 16)

#define MPU_GUARD_REGION_FIRST 0U
#define MPU_REGION_COUNT 16U
#define MPU_GUARD_REGION_COUNT (MPU_REGION_COUNT - MPU_GUARD_REGION_FIRST)

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
static task_storage_t task_storage[KERNEL_MAX_TASKS] TASK_UNPRIVILEGED_DATA;
static task_t tasks[KERNEL_MAX_TASKS] KERNEL_PRIVILEGED_DATA = { 0U };
static task_t *current_task KERNEL_PRIVILEGED_DATA = &tasks[0];
static uint32_t task_count KERNEL_PRIVILEGED_DATA;
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
    saved_primask = critical_enter();
    *state = (task_state_t)tasks[task_id].state;
    critical_exit(saved_primask);
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
    saved_primask = critical_enter();
    task = &tasks[task_id];
    info->stack_words = (uint32_t)(task->stack_top - task->stack_bottom);
    info->used_words = task->high_water_words;
    info->minimum_sp = (uint32_t)(uintptr_t)task->minimum_sp;
    info->current_sp = (uint32_t)(uintptr_t)task->sp;
    critical_exit(saved_primask);
    return KERNEL_OK;
}

kernel_status_t task_get_name(uint32_t task_id, const char **name)
{
    uint32_t saved_primask;

    if (name == 0U || validate_task_id(task_id) != KERNEL_OK)
    {
        return KERNEL_ERR_INVALID_TASK;
    }
    saved_primask = critical_enter();
    *name = tasks[task_id].name;
    critical_exit(saved_primask);
    return KERNEL_OK;
}

kernel_status_t task_get_priority(uint32_t task_id, uint32_t *priority)
{
    uint32_t saved_primask;

    if (priority == 0U || validate_task_id(task_id) != KERNEL_OK)
    {
        return KERNEL_ERR_INVALID_TASK;
    }
    saved_primask = critical_enter();
    *priority = tasks[task_id].priority;
    critical_exit(saved_primask);
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
    uint32_t saved_primask = critical_enter();
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

    critical_exit(saved_primask);
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
    uint32_t saved_primask = critical_enter();
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
    critical_exit(saved_primask);
}

static void configure_stack_guards(void)
{
    uint32_t index;

    MPU_CTRL = 0U;
    SCB_SHCSR |= SCB_SHCSR_MEMFAULTENA;
    for (index = 0U; index < task_count; index++)
    {
        MPU_RNR = index + MPU_GUARD_REGION_FIRST;
        MPU_RBAR = (uint32_t)(uintptr_t)&task_storage[index].guard[0];
        MPU_RASR = MPU_RASR_XN | MPU_RASR_SIZE_32_BYTES | MPU_RASR_ENABLE;
    }
    MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
    __asm volatile ("dsb\nisb" : : : "memory");
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
}

void sleep_current(uint32_t ticks)
{
    uint32_t saved_primask = critical_enter();

    current_task->sleep_ticks = ticks;
    current_task->state = (ticks == 0U) ? TASK_STATE_READY : TASK_STATE_SLEEPING;
    critical_exit(saved_primask);
}

int task_block(void *object, task_wait_kind_t wait_kind, uint32_t timeout_ticks)
{
    uint32_t saved_primask;

    if (timeout_ticks == 0U)
    {
        return 0;
    }

    saved_primask = critical_enter();
    current_task->wait_object = object;
    current_task->wait_kind = wait_kind;
    current_task->wait_ticks = timeout_ticks;
    current_task->wait_result = 0U;
    current_task->state = TASK_STATE_BLOCKED;
    critical_exit(saved_primask);
    yield();
    return (int)current_task->wait_result;
}

void task_wake(void *object, task_wait_kind_t wait_kind)
{
    uint32_t saved_primask = critical_enter();
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
        tasks[selected_index].state = TASK_STATE_READY;
        tasks[selected_index].wait_result = 1U;
        tasks[selected_index].wait_object = 0U;
        tasks[selected_index].wait_kind = TASK_WAIT_NONE;
    }

    critical_exit(saved_primask);
}

void tick_tasks(void)
{
    uint32_t saved_primask = critical_enter();
    uint32_t index;

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

                tasks[index].state = TASK_STATE_READY;
                tasks[index].wait_result = 0U;
                tasks[index].wait_object = 0U;
                tasks[index].wait_kind = TASK_WAIT_NONE;

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

    critical_exit(saved_primask);
}

uint32_t *pendsv_switch(uint32_t *current_sp)
{
    uint32_t saved_primask = critical_enter();
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
    critical_exit(saved_primask);
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
        "movs    r0,  #2                \n"
        "msr     control, r0            \n"
        "isb                            \n"
        "cpsie   i                      \n"
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
    if ((config->task_count + 1U) > MPU_GUARD_REGION_COUNT)
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
    configure_stack_guards();
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
    tick_init();
    launch_first_task(current_task->sp);
}
