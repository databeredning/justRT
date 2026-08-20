#include <stdint.h>

#include "kernel.h"
#include "waiter_priority_wake.h"

#define HIGH_WAITER_PRIORITY 3U
#define LOW_WAITER_PRIORITY 2U
#define CONTROLLER_PRIORITY 1U
#define WAKE_HIGH_MARKER 0xA1U
#define WAKE_LOW_MARKER 0xB2U

static semaphore_t wake_semaphore;
volatile uint32_t g_waiter_wake_order[2];
volatile uint32_t g_waiter_wake_count;
volatile uint32_t g_waiter_wake_error;
volatile uint32_t g_waiter_wake_done;

static TASK_UNPRIVILEGED void high_waiter_task(void *argument)
{
    (void)argument;

    if (semaphore_take(&wake_semaphore, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_waiter_wake_error = 1U;
    }
    else if (g_waiter_wake_count < 2U)
    {
        g_waiter_wake_order[g_waiter_wake_count] = WAKE_HIGH_MARKER;
        g_waiter_wake_count++;
    }
    else
    {
        g_waiter_wake_error = 2U;
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void low_waiter_task(void *argument)
{
    (void)argument;

    if (semaphore_take(&wake_semaphore, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_waiter_wake_error = 3U;
    }
    else if (g_waiter_wake_count < 2U)
    {
        g_waiter_wake_order[g_waiter_wake_count] = WAKE_LOW_MARKER;
        g_waiter_wake_count++;
    }
    else
    {
        g_waiter_wake_error = 4U;
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void controller_task(void *argument)
{
    (void)argument;

    sleep_ticks(1U);

    semaphore_give(&wake_semaphore);
    sleep_ticks(1U);

    if ((g_waiter_wake_count != 1U)
        || (g_waiter_wake_order[0] != WAKE_HIGH_MARKER))
    {
        g_waiter_wake_error = 5U;
    }

    semaphore_give(&wake_semaphore);
    sleep_ticks(1U);

    if ((g_waiter_wake_count != 2U)
        || (g_waiter_wake_order[1] != WAKE_LOW_MARKER))
    {
        g_waiter_wake_error = 6U;
    }

    g_waiter_wake_done = 1U;

    while (1)
    {
        sleep_ticks(1U);
    }
}

static const task_definition_t wake_test_tasks[] = {
    { high_waiter_task, 0U, KERNEL_TASK_STACK_WORDS, HIGH_WAITER_PRIORITY, "wake-high", 0U },
    { low_waiter_task, 0U, KERNEL_TASK_STACK_WORDS, LOW_WAITER_PRIORITY, "wake-low", 0U },
    { controller_task, 0U, KERNEL_TASK_STACK_WORDS, CONTROLLER_PRIORITY, "wake-control", 0U }
};

void waiter_priority_wake_start(void)
{
    const kernel_config_t config = {
        wake_test_tasks,
        sizeof(wake_test_tasks) / sizeof(wake_test_tasks[0])
    };

    semaphore_init(&wake_semaphore, 0U);
    g_waiter_wake_order[0] = 0U;
    g_waiter_wake_order[1] = 0U;
    g_waiter_wake_count = 0U;
    g_waiter_wake_error = 0U;
    g_waiter_wake_done = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
