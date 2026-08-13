#include <stdint.h>

extern void kernel_tick_init(void);
extern void kernel_dispatch_from_pendsv(void);

typedef void (*kernel_task_entry_t)(void);

typedef struct
{
    uint32_t stack_top;
    uint32_t stack_bottom;
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

static kernel_task_t g_tasks[2] = {
    { 0U, 0U, 0U, 0U, 0U },
    { 0U, 0U, 0U, 0U, 0U }
};
static kernel_task_t *g_current_task = &g_tasks[0];

static int platform_sanity_check(void)
{
    return ((g_initialized_value == 0x12345678U) && (g_uninitialized_value == 0U)) ? 0 : -1;
}

static void task0_step(void)
{
    g_active_task_tag = 0xA0U;
    g_task0_runs++;
    g_boot_counter++;
}

static void task1_step(void)
{
    g_active_task_tag = 0xB1U;
    g_task1_runs++;
    g_boot_counter++;
}

static void kernel_idle_loop(void)
{
    g_boot_stage = 5U;

    while (1)
    {
        __asm volatile ("wfi" : : : "memory");
    }
}

void kernel_dispatch_from_pendsv(void)
{
    g_current_task = &g_tasks[g_current_task_index];
    g_current_task->state = 1U;
    g_current_task->entry();
    g_current_task->run_count++;
    g_schedule_count++;
    g_current_task_index ^= 1U;
}

static void kernel_enter(void)
{
    g_tasks[0].state = 1U;
    g_tasks[0].entry = task0_step;
    g_tasks[1].state = 1U;
    g_tasks[1].entry = task1_step;
    g_current_task = &g_tasks[0];
    g_current_task_index = 0U;
    g_kernel_started = 1U;
    g_boot_stage = 4U;
    kernel_tick_init();
    __asm volatile ("cpsie i" : : : "memory");
    kernel_idle_loop();
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
