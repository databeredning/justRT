#include <stdint.h>

extern void kernel_tick_init(void);
extern uint32_t *kernel_pendsv_switch(uint32_t *current_sp);

volatile uint32_t g_yield_count = 0U;

static inline void kernel_yield(void)
{
    __asm volatile ("svc 0" : : : "memory");
}

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

static __attribute__((aligned(8))) uint32_t g_task0_stack[128];
static __attribute__((aligned(8))) uint32_t g_task1_stack[128];
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

    /* Hardware exception frame restored by exception return. */
    *--stack = 0x01000000U;
    *--stack = ((uint32_t)entry) & ~1U;        /* PC: even instruction address; T from xPSR */
    *--stack = ((uint32_t)task_exit_trap) | 1U; /* LR: bit[0]=1 for Thumb BX on task return */
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;
    *--stack = 0U;

    /* Software-saved frame restored by PendSV (r4-r11). */
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

static void task0_body(void)
{
    while (1)
    {
        g_active_task_tag = 0xA0U;
        g_task0_runs++;
        g_boot_counter++;
        if ((g_task0_runs & 0xFFU) == 0U)
        {
            kernel_yield();
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
            kernel_yield();
        }
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

/* Restores task state from its initial stack exactly as PendSV does, then enters the task. */
__attribute__((naked)) static void kernel_launch_first_task(uint32_t *sp __attribute__((unused)))
{
    __asm volatile (
        /* sp arrives in r0; frame layout matches what kernel_build_initial_stack produced */
        "ldmia   r0!, {r4-r11}          \n"  /* restore software frame; r0 now at hardware frame */
        "ldr     lr,  [r0, #20]         \n"  /* lr  = stacked LR (task_exit_trap) */
        "ldr     r2,  [r0, #24]         \n"  /* r2  = stacked PC (task entry) */
        "orr     r2,  r2, #1            \n"  /* direct branch needs a Thumb address */
        "adds    r0,  r0, #32           \n"  /* advance past 8-word hardware frame */
        "msr     psp, r0                \n"  /* PSP = live task stack top */
        "movs    r0,  #2                \n"
        "msr     control, r0            \n"  /* CONTROL.SPSEL=1: Thread mode uses PSP */
        "isb                            \n"
        "cpsie   i                      \n"
        "movs    r0,  #0                \n"
        "movs    r1,  #0                \n"
        "movs    r3,  #0                \n"
        "bx      r2                     \n"  /* enter task; lr = task_exit_trap */
    );
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
    kernel_launch_first_task(g_current_task->sp); /* never returns */
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
