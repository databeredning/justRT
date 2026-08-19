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
    TASK_SLEEPING = 2U
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
    { 0U, 0U, 0U, TASK_READY, 0U, 0U, 0U, 0U, 0U },
    { 0U, 0U, 0U, TASK_READY, 0U, 0U, 0U, 0U, 0U },
    { 0U, 0U, 0U, TASK_READY, 0U, 0U, 0U, 0U, 0U }
};
static task_t *current_task = &tasks[0];
static uint32_t task_count;

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

static uint32_t *build_initial_stack(uint32_t *stack_top, task_entry_t entry)
{
    uint32_t *stack = stack_top;

    *--stack = 0x01000000U;
    *--stack = ((uint32_t)entry) & ~1U;
    *--stack = ((uint32_t)task_exit_trap) | 1U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;

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

static void idle_body(void)
{
    while (1)
    {
        __asm volatile ("wfi" : : : "memory");
    }
}

static void prepare_task(uint32_t index, task_entry_t entry)
{
    task_storage_t *storage = (index == 0U) ? &task0_storage : &task1_storage;

    fill_stack(&storage->stack[0], &storage->stack[TASK_STACK_WORDS]);
    tasks[index].stack_bottom = &storage->stack[0];
    tasks[index].stack_top = &storage->stack[TASK_STACK_WORDS];
    tasks[index].sp = build_initial_stack(tasks[index].stack_top, entry);
    tasks[index].state = TASK_READY;
    tasks[index].minimum_sp = tasks[index].sp;
    tasks[index].high_water_words = 16U;
    tasks[index].entry = entry;
}

static void prepare_idle_task(void)
{
    fill_stack(&idle_storage.stack[0], &idle_storage.stack[TASK_STACK_WORDS]);
    tasks[TASK_IDLE_INDEX].stack_bottom = &idle_storage.stack[0];
    tasks[TASK_IDLE_INDEX].stack_top = &idle_storage.stack[TASK_STACK_WORDS];
    tasks[TASK_IDLE_INDEX].sp = build_initial_stack(
        tasks[TASK_IDLE_INDEX].stack_top, idle_body);
    tasks[TASK_IDLE_INDEX].state = TASK_READY;
    tasks[TASK_IDLE_INDEX].minimum_sp = tasks[TASK_IDLE_INDEX].sp;
    tasks[TASK_IDLE_INDEX].high_water_words = 16U;
    tasks[TASK_IDLE_INDEX].entry = idle_body;
}

void sleep_current(uint32_t ticks)
{
    uint32_t saved_primask = critical_enter();

    current_task->sleep_ticks = ticks;
    current_task->state = (ticks == 0U) ? TASK_READY : TASK_SLEEPING;
    critical_exit(saved_primask);
}

void tick_tasks(void)
{
    uint32_t saved_primask = critical_enter();
    uint32_t index;

    for (index = 0U; index <= TASK_IDLE_INDEX; index++)
    {
        if ((tasks[index].state == TASK_SLEEPING) && (tasks[index].sleep_ticks > 0U))
        {
            tasks[index].sleep_ticks--;
            if (tasks[index].sleep_ticks == 0U)
            {
                tasks[index].state = TASK_READY;
            }
        }
    }

    critical_exit(saved_primask);
}

uint32_t *pendsv_switch(uint32_t *current_sp)
{
    uint32_t saved_primask = critical_enter();
    uint32_t offset;
    uint32_t next_index = g_current_task_index;

    current_task->sp = current_sp;
    update_stack_usage(current_task, current_sp);
    current_task->run_count++;
    if (current_task->state == TASK_RUNNING)
    {
        current_task->state = TASK_READY;
    }

    for (offset = 1U; offset <= task_count; offset++)
    {
        next_index = (g_current_task_index + offset) % task_count;
        if (tasks[next_index].state != TASK_SLEEPING)
        {
            break;
        }
    }

    g_current_task_index = next_index;
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

void kernel_start(const task_config_t *config)
{
    if ((config == 0U) || (config->entries == 0U)
        || (config->count == 0U) || (config->count >= TASK_MAX_TASKS))
    {
        while (1)
        {
        }
    }

    task_count = config->count + 1U;
    for (uint32_t index = 0U; index < config->count; index++)
    {
        prepare_task(index, config->entries[index]);
    }
    prepare_idle_task();
    configure_stack_guards();
    current_task = &tasks[0];
    g_current_task_index = 0U;
    tick_init();
    launch_first_task(current_task->sp);
}
