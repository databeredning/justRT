#include <stdint.h>

#include "kernel.h"
#include "mutex_edge_cases.h"

#define MUTEX_EDGE_HOLD_TIME_MS 20U

static mutex_t test_mutex;
volatile uint32_t g_mutex_recursive_first_lock;
volatile uint32_t g_mutex_recursive_second_lock;
volatile uint32_t g_mutex_recursive_first_unlock;
volatile uint32_t g_mutex_recursive_second_unlock;
volatile uint32_t g_mutex_non_owner_unlock;
volatile uint32_t g_mutex_edge_error;

static TASK_UNPRIVILEGED void mutex_edge_task(void *argument)
{
    (void)argument;

    g_mutex_recursive_first_lock =
        (uint32_t)mutex_lock(&test_mutex, SEMAPHORE_WAIT_FOREVER);
    g_mutex_recursive_second_lock =
        (uint32_t)mutex_lock(&test_mutex, SEMAPHORE_WAIT_FOREVER);
    sleep_ticks(ms_to_ticks(MUTEX_EDGE_HOLD_TIME_MS));
    g_mutex_recursive_first_unlock = (uint32_t)mutex_unlock(&test_mutex);
    g_mutex_recursive_second_unlock = (uint32_t)mutex_unlock(&test_mutex);

    if ((g_mutex_recursive_first_lock != 1U)
        || (g_mutex_recursive_second_lock != 1U)
        || (g_mutex_recursive_first_unlock != 1U)
        || (g_mutex_recursive_second_unlock != 1U))
    {
        g_mutex_edge_error = 1U;
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void non_owner_task(void *argument)
{
    (void)argument;
    g_mutex_non_owner_unlock = (uint32_t)mutex_unlock(&test_mutex);
    if (g_mutex_non_owner_unlock != 0U)
    {
        g_mutex_edge_error = 2U;
    }
    while (1)
    {
        sleep_ticks(1U);
    }
}

static const task_definition_t mutex_edge_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { mutex_edge_task, 0U, KERNEL_TASK_STACK_WORDS, 2U, "mutex-edge-owner", 0U },
    { non_owner_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "mutex-edge-non-owner", 0U }
};

void mutex_edge_cases_start(void)
{
    const kernel_config_t config = {
        mutex_edge_tasks,
        sizeof(mutex_edge_tasks) / sizeof(mutex_edge_tasks[0])
    };

    mutex_init(&test_mutex);
    g_mutex_recursive_first_lock = 0U;
    g_mutex_recursive_second_lock = 0U;
    g_mutex_recursive_first_unlock = 0U;
    g_mutex_recursive_second_unlock = 0U;
    g_mutex_non_owner_unlock = 0U;
    g_mutex_edge_error = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
