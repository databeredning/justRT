#include <stdint.h>

#include "kernel.h"

#define TASK_MAX_TASKS 3U
#define TASK_IDLE_INDEX (TASK_MAX_TASKS - 1U)
#define TASK_STACK_WORDS 128U
#define TASK_STACK_FILL 0xA5A5A5A5U
#define TASK_GUARD_WORDS 8U
#define RUN_LED_PERIOD_MS 100U

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

enum
{
    TASK_READY = 0U,
    TASK_RUNNING = 1U,
    TASK_SLEEPING = 2U,
    TASK_BLOCKED = 3U
};

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
    const char *name;
    uint32_t flags;
    void *wait_object;
    task_wait_kind_t wait_kind;
    uint32_t wait_ticks;
    uint32_t wait_result;
} task_t;

typedef struct __attribute__((aligned(32)))
{
    uint32_t guard[TASK_GUARD_WORDS];
    uint32_t stack[TASK_STACK_WORDS];
} task_storage_t;

volatile uint32_t g_current_task_index = 0U;
volatile uint32_t g_stack_fault = 0U;
volatile uint32_t g_stack_fault_task = 0U;
volatile uint32_t g_stack_fault_sp = 0U;
static task_storage_t task0_storage;
static task_storage_t task1_storage;
static task_storage_t idle_storage;
static task_t tasks[TASK_MAX_TASKS] = {
    { 0U },
    { 0U },
    { 0U }
};
static task_t *current_task = &tasks[0];
static uint32_t task_count;
static uint32_t kernel_initialized;

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

static void configure_stack_guards(void)
{
    const task_storage_t *storage[TASK_MAX_TASKS] = {
        &task0_storage,
        &task1_storage,
        &idle_storage
    };
    uint32_t index;

    MPU_CTRL = 0U;
    SCB_SHCSR |= SCB_SHCSR_MEMFAULTENA;
    for (index = 0U; index < task_count; index++)
    {
        MPU_RNR = index + MPU_GUARD_REGION_FIRST;
        MPU_RBAR = (uint32_t)(uintptr_t)&storage[index]->guard[0];
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
        *word = TASK_STACK_FILL;
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
    while ((word < task->stack_top) && (*word != TASK_STACK_FILL))
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
        __asm volatile ("wfi" : : : "memory");
    }
}

static void prepare_task(uint32_t index, const task_definition_t *definition)
{
    task_storage_t *storage = (index == 0U) ? &task0_storage : &task1_storage;
    uint32_t *stack_bottom = &storage->stack[0];
    uint32_t *stack_top = &storage->stack[definition->stack_words];

    fill_stack(stack_bottom, stack_top);
    tasks[index].stack_bottom = stack_bottom;
    tasks[index].stack_top = stack_top;
    tasks[index].sp = build_initial_stack(stack_top, definition->entry,
                                          definition->argument);
    tasks[index].state = TASK_READY;
    tasks[index].minimum_sp = tasks[index].sp;
    tasks[index].high_water_words = 16U;
    tasks[index].entry = definition->entry;
    tasks[index].argument = definition->argument;
    tasks[index].priority = definition->priority;
    tasks[index].name = definition->name;
    tasks[index].flags = definition->flags;
}

static void prepare_idle_task(void)
{
    fill_stack(&idle_storage.stack[0], &idle_storage.stack[TASK_STACK_WORDS]);
    tasks[TASK_IDLE_INDEX].stack_bottom = &idle_storage.stack[0];
    tasks[TASK_IDLE_INDEX].stack_top = &idle_storage.stack[TASK_STACK_WORDS];
    tasks[TASK_IDLE_INDEX].sp = build_initial_stack(
        tasks[TASK_IDLE_INDEX].stack_top, idle_body, 0U);
    tasks[TASK_IDLE_INDEX].state = TASK_READY;
    tasks[TASK_IDLE_INDEX].minimum_sp = tasks[TASK_IDLE_INDEX].sp;
    tasks[TASK_IDLE_INDEX].high_water_words = 16U;
    tasks[TASK_IDLE_INDEX].entry = idle_body;
    tasks[TASK_IDLE_INDEX].argument = 0U;
    tasks[TASK_IDLE_INDEX].priority = 0U;
    tasks[TASK_IDLE_INDEX].name = "idle";
    tasks[TASK_IDLE_INDEX].flags = 0U;
}

void sleep_current(uint32_t ticks)
{
    uint32_t saved_primask = critical_enter();

    current_task->sleep_ticks = ticks;
    current_task->state = (ticks == 0U) ? TASK_READY : TASK_SLEEPING;
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
    current_task->state = TASK_BLOCKED;
    critical_exit(saved_primask);
    yield();
    return (int)current_task->wait_result;
}

void task_wake(void *object, task_wait_kind_t wait_kind)
{
    uint32_t saved_primask = critical_enter();
    uint32_t index;

    for (index = 0U; index < task_count; index++)
    {
        if ((tasks[index].state == TASK_BLOCKED)
            && (tasks[index].wait_object == object)
            && (tasks[index].wait_kind == wait_kind))
        {
            tasks[index].state = TASK_READY;
            tasks[index].wait_result = 1U;
            tasks[index].wait_object = 0U;
            tasks[index].wait_kind = TASK_WAIT_NONE;
            break;
        }
    }

    critical_exit(saved_primask);
}

void tick_tasks(void)
{
    uint32_t saved_primask = critical_enter();
    uint32_t index;

    for (index = 0U; index < task_count; index++)
    {
        if ((tasks[index].state == TASK_SLEEPING) && (tasks[index].sleep_ticks > 0U))
        {
            tasks[index].sleep_ticks--;
            if (tasks[index].sleep_ticks == 0U)
            {
                tasks[index].state = TASK_READY;
            }
        }
        else if ((tasks[index].state == TASK_BLOCKED)
            && (tasks[index].wait_ticks != SEMAPHORE_WAIT_FOREVER)
            && (tasks[index].wait_ticks > 0U))
        {
            tasks[index].wait_ticks--;
            if (tasks[index].wait_ticks == 0U)
            {
                tasks[index].state = TASK_READY;
                tasks[index].wait_result = 0U;
                tasks[index].wait_object = 0U;
                tasks[index].wait_kind = TASK_WAIT_NONE;
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

    current_task->sp = current_sp;
    update_stack_usage(current_task, current_sp);
    current_task->run_count++;
    if (current_task->state == TASK_RUNNING)
    {
        current_task->state = TASK_READY;
    }

    for (offset = 0U; offset < task_count; offset++)
    {
        next_index = (base_index + offset) % task_count;
        if ((tasks[next_index].state == TASK_READY)
            && (tasks[next_index].priority > best_priority))
        {
            best_priority = tasks[next_index].priority;
        }
    }

    for (offset = 1U; offset <= task_count; offset++)
    {
        next_index = (base_index + offset) % task_count;
        if ((tasks[next_index].state == TASK_READY)
            && (tasks[next_index].priority == best_priority))
        {
            g_current_task_index = next_index;
            break;
        }
    }

    current_task = &tasks[g_current_task_index];
    current_task->state = TASK_RUNNING;
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
    if (config->task_count >= TASK_MAX_TASKS)
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
            || (definition->stack_words > TASK_STACK_WORDS)
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
    current_task->state = TASK_RUNNING;
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
