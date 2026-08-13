#include <stdint.h>

extern void kernel_tick_init(void);
extern uint32_t *kernel_pendsv_switch(uint32_t *current_sp);

typedef void (*kernel_task_entry_t)(void);

typedef struct
{
    uint32_t *stack_bottom;
    uint32_t *stack_top;
    uint32_t *sp;
    uint32_t state;
    uint32_t run_count;
    kernel_task_entry_t entry;
} kernel_task_t;

volatile uint32_t g_boot_counter = 0U;
volatile uint32_t g_main_entered = 0U;
volatile uint32_t g_boot_stage = 0U;
volatile uint32_t g_kernel_started = 0U;
volatile uint32_t g_current_task_index = 0U;
volatile uint32_t g_schedule_count = 0U;
volatile uint32_t g_task0_runs = 0U;
volatile uint32_t g_task1_runs = 0U;
volatile uint32_t g_active_task_tag = 0U;
const uint32_t g_initialized_value = 0x12345678U;
uint32_t g_uninitialized_value;

static uint32_t g_task0_stack[128];
static uint32_t g_task1_stack[128];
static kernel_task_t g_tasks[2] = {
    { 0U, 0U, 0U, 0U, 0U, 0U },
    { 0U, 0U, 0U, 0U, 0U, 0U }
};
static kernel_task_t *g_current_task = &g_tasks[0];

static int platform_sanity_check(void)
{
    return ((g_initialized_value == 0x12345678U) && (g_uninitialized_value == 0U)) ? 0 : -1;
}

static void task_exit_trap(void)
{
    while (1)
    {
    }
}

static uint32_t *kernel_build_initial_stack(uint32_t *stack_top, kernel_task_entry_t entry)
{
    uint32_t *stack = stack_top;

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
    *--stack = ((uint32_t)task_exit_trap) | 1U;
    *--stack = ((uint32_t)entry) | 1U;
    *--stack = 0x01000000U;

    return stack;
}

static void task0_body(void)
{
    while (1)
    {
        g_active_task_tag = 0xA0U;
        g_task0_runs++;
        g_boot_counter++;
    }
}

static void task1_body(void)
{
    while (1)
    {
        g_active_task_tag = 0xB1U;
        g_task1_runs++;
        g_boot_counter++;
    }
}

static void kernel_prepare_tasks(void)
{
    g_tasks[0].stack_bottom = &g_task0_stack[0];
    g_tasks[0].stack_top = &g_task0_stack[128];
    g_tasks[0].sp = kernel_build_initial_stack(g_tasks[0].stack_top, task0_body);
    g_tasks[0].state = 1U;
    g_tasks[0].entry = task0_body;

    g_tasks[1].stack_bottom = &g_task1_stack[0];
    g_tasks[1].stack_top = &g_task1_stack[128];
    g_tasks[1].sp = kernel_build_initial_stack(g_tasks[1].stack_top, task1_body);
    g_tasks[1].state = 1U;
    g_tasks[1].entry = task1_body;
}

uint32_t *kernel_pendsv_switch(uint32_t *current_sp)
{
    g_current_task->sp = current_sp;
    g_current_task->run_count++;
    g_schedule_count++;
    g_current_task_index ^= 1U;
    g_current_task = &g_tasks[g_current_task_index];
    g_current_task->state = 1U;
    return g_current_task->sp;
}

static void kernel_enter(void)
{
    kernel_prepare_tasks();
    g_current_task = &g_tasks[0];
    g_current_task_index = 0U;
    g_kernel_started = 1U;
    g_boot_stage = 4U;
    kernel_tick_init();
    g_boot_stage = 5U;

    __asm volatile (
        "msr psp, %0\n"
        "mrs r0, control\n"
        "orr r0, r0, #2\n"
        "msr control, r0\n"
        "isb\n"
        :
        : "r" (g_tasks[0].sp)
        : "r0", "memory");

    __asm volatile ("cpsie i" : : : "memory");
    g_tasks[0].entry();
}

int main(void)
{
    g_boot_stage = 1U;
    g_main_entered = 1U;
    if (platform_sanity_check() != 0)
    {
        g_boot_stage = 0xEEU;
        while (1)
        {
        }
    }

    g_boot_stage = 2U;
    kernel_enter();

    return 0;
}
