#include <stdint.h>

#include "kernel.h"
#include "waiter_timeout_wake.h"

#define TIMEOUT_WAITER_PRIORITY 3U
#define REMAINING_HIGH_PRIORITY 2U
#define REMAINING_LOW_PRIORITY 1U
#define TIMEOUT_TICKS 2U
#define WAKE_REMAINING_HIGH_MARKER 0xC3U
#define WAKE_REMAINING_LOW_MARKER 0xD4U

static semaphore_t wake_semaphore;
volatile uint32_t g_waiter_timeout_flag;
volatile uint32_t g_waiter_timeout_wake_order[2] TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_waiter_timeout_wake_count;
volatile uint32_t g_waiter_timeout_error;
volatile uint32_t g_waiter_timeout_done;

static TASK_UNPRIVILEGED void timeout_then_give_task(void *argument)
{
    (void)argument;

    if (semaphore_take(&wake_semaphore, TIMEOUT_TICKS) != 0)
    {
        g_waiter_timeout_error = 1U;
    }
    else
    {
        g_waiter_timeout_flag = 1U;
    }

    semaphore_give(&wake_semaphore);
    sleep_ticks(1U);

    if (g_waiter_timeout_flag != 1U)
    {
        g_waiter_timeout_error = 2U;
    }
    else if ((g_waiter_timeout_wake_count != 1U)
        || (g_waiter_timeout_wake_order[0] != WAKE_REMAINING_HIGH_MARKER))
    {
        g_waiter_timeout_error = 3U;
    }

    g_waiter_timeout_done = 1U;

    while (1)
    {
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void remaining_high_waiter_task(void *argument)
{
    (void)argument;

    if (semaphore_take(&wake_semaphore, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_waiter_timeout_error = 4U;
    }
    else if (g_waiter_timeout_wake_count < 2U)
    {
        g_waiter_timeout_wake_order[g_waiter_timeout_wake_count] = WAKE_REMAINING_HIGH_MARKER;
        g_waiter_timeout_wake_count++;
    }
    else
    {
        g_waiter_timeout_error = 5U;
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void remaining_low_waiter_task(void *argument)
{
    (void)argument;

    if (semaphore_take(&wake_semaphore, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_waiter_timeout_error = 6U;
    }
    else if (g_waiter_timeout_wake_count < 2U)
    {
        g_waiter_timeout_wake_order[g_waiter_timeout_wake_count] = WAKE_REMAINING_LOW_MARKER;
        g_waiter_timeout_wake_count++;
    }
    else
    {
        g_waiter_timeout_error = 7U;
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static const task_definition_t wake_timeout_test_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { timeout_then_give_task, 0U, KERNEL_TASK_STACK_WORDS, TIMEOUT_WAITER_PRIORITY, "timeout-give", 0U },
    { remaining_high_waiter_task, 0U, KERNEL_TASK_STACK_WORDS, REMAINING_HIGH_PRIORITY, "remain-high", 0U },
    { remaining_low_waiter_task, 0U, KERNEL_TASK_STACK_WORDS, REMAINING_LOW_PRIORITY, "remain-low", 0U }
};

void waiter_timeout_wake_start(void)
{
    const kernel_config_t config = {
        wake_timeout_test_tasks,
        sizeof(wake_timeout_test_tasks) / sizeof(wake_timeout_test_tasks[0])
    };

    semaphore_init(&wake_semaphore, 0U);
    g_waiter_timeout_flag = 0U;
    g_waiter_timeout_wake_order[0] = 0U;
    g_waiter_timeout_wake_order[1] = 0U;
    g_waiter_timeout_wake_count = 0U;
    g_waiter_timeout_error = 0U;
    g_waiter_timeout_done = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
