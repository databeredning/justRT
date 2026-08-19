#include <stdint.h>

#include "kernel.h"

typedef void (*task_entry_t)(void);

#define TASK_STACK_WORDS 128U
#define TASK_STACK_FILL 0xA5A5A5A5U

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

volatile uint32_t g_schedule_count = 0U;
volatile uint32_t g_task0_runs = 0U;
volatile uint32_t g_task1_runs = 0U;
volatile uint32_t g_active_task_tag = 0U;
volatile uint32_t g_boot_counter = 0U;
volatile uint32_t g_boot_stage = 0U;
volatile uint32_t g_kernel_started = 0U;
volatile uint32_t g_current_task_index = 0U;
volatile uint32_t g_stack_fault = 0U;
volatile uint32_t g_stack_fault_task = 0U;
volatile uint32_t g_stack_fault_sp = 0U;
static uint32_t task0_stack[TASK_STACK_WORDS] __attribute__((aligned(8)));
static uint32_t task1_stack[TASK_STACK_WORDS] __attribute__((aligned(8)));
static uint32_t idle_stack[TASK_STACK_WORDS] __attribute__((aligned(8)));
static task_t tasks[3] = {
    { 0U, 0U, 0U, TASK_READY, 0U, 0U, 0U, 0U, 0U },
    { 0U, 0U, 0U, TASK_READY, 0U, 0U, 0U, 0U, 0U },
    { 0U, 0U, 0U, TASK_READY, 0U, 0U, 0U, 0U, 0U }
};
static task_t *current_task = &tasks[0];

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

static void task0_body(void)
{
    while (1)
    {
        g_active_task_tag = 0xA0U;
        g_task0_runs++;
        g_boot_counter++;
        if ((g_task0_runs & 0xFFU) == 0U)
        {
            yield();
        }
    }
}

static void task1_body(void)
{
    while (1)
    {
        g_active_task_tag = 0xB1U;
        g_task1_runs++;
        g_boot_counter++;
        if ((g_task1_runs & 0xFFU) == 0U)
        {
            sleep_ticks(7U);
        }
    }
}

static void idle_body(void)
{
    while (1)
    {
        __asm volatile ("wfi" : : : "memory");
    }
}

static void prepare_tasks(void)
{
    fill_stack(&task0_stack[0], &task0_stack[TASK_STACK_WORDS]);
    tasks[0].stack_bottom = &task0_stack[0];
    tasks[0].stack_top = &task0_stack[TASK_STACK_WORDS];
    tasks[0].sp = build_initial_stack(tasks[0].stack_top, task0_body);
    tasks[0].state = TASK_READY;
    tasks[0].minimum_sp = tasks[0].sp;
    tasks[0].high_water_words = 16U;
    tasks[0].entry = task0_body;

    fill_stack(&task1_stack[0], &task1_stack[TASK_STACK_WORDS]);
    tasks[1].stack_bottom = &task1_stack[0];
    tasks[1].stack_top = &task1_stack[TASK_STACK_WORDS];
    tasks[1].sp = build_initial_stack(tasks[1].stack_top, task1_body);
    tasks[1].state = TASK_READY;
    tasks[1].minimum_sp = tasks[1].sp;
    tasks[1].high_water_words = 16U;
    tasks[1].entry = task1_body;

    fill_stack(&idle_stack[0], &idle_stack[TASK_STACK_WORDS]);
    tasks[2].stack_bottom = &idle_stack[0];
    tasks[2].stack_top = &idle_stack[TASK_STACK_WORDS];
    tasks[2].sp = build_initial_stack(tasks[2].stack_top, idle_body);
    tasks[2].state = TASK_READY;
    tasks[2].minimum_sp = tasks[2].sp;
    tasks[2].high_water_words = 16U;
    tasks[2].entry = idle_body;
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

    for (index = 0U; index < 3U; index++)
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
    g_schedule_count++;
    if (current_task->state == TASK_RUNNING)
    {
        current_task->state = TASK_READY;
    }

    for (offset = 1U; offset <= 3U; offset++)
    {
        next_index = (g_current_task_index + offset) % 3U;
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

void start(void)
{
    g_boot_stage = 4U;
    prepare_tasks();
    current_task = &tasks[0];
    g_current_task_index = 0U;
    g_kernel_started = 1U;
    tick_init();
    g_boot_stage = 5U;
    launch_first_task(current_task->sp);
}
