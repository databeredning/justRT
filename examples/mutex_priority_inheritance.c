#include <stdint.h>

#include "kernel.h"
#include "mutex_priority_inheritance.h"

#define LOW_TASK_ID 0U
#define HIGH_TASK_ID 1U
#define MEDIUM_TASK_ID 2U
#define LOW_BASE_PRIORITY 1U
#define MEDIUM_PRIORITY 2U
#define HIGH_PRIORITY 3U
#define HOLD_TIME_MS 20U

static mutex_t shared_mutex;
volatile uint32_t g_inheritance_low_priority;
volatile uint32_t g_inheritance_high_state;
volatile uint32_t g_inheritance_low_operations;
volatile uint32_t g_inheritance_high_operations;
volatile uint32_t g_inheritance_medium_operations;
volatile uint32_t g_inheritance_error;

static void low_owner_task(void *argument)
{
    uint32_t priority;

    (void)argument;
    while (1)
    {
        if (mutex_lock(&shared_mutex, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_inheritance_error = 1U;
            continue;
        }

        if (task_get_priority(LOW_TASK_ID, &priority) != KERNEL_OK)
        {
            g_inheritance_error = 2U;
        }
        else
        {
            g_inheritance_low_priority = priority;
        }
        g_inheritance_low_operations++;
        sleep_ticks(ms_to_ticks(HOLD_TIME_MS));
        mutex_unlock(&shared_mutex);
        sleep_ticks(1U);
    }
}

static void high_waiter_task(void *argument)
{
    task_state_t state;

    (void)argument;
    while (1)
    {
        if (mutex_lock(&shared_mutex, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_inheritance_error = 3U;
            continue;
        }

        if (task_get_state(LOW_TASK_ID, &state) != KERNEL_OK)
        {
            g_inheritance_error = 4U;
        }
        else
        {
            g_inheritance_high_state = (uint32_t)state;
        }
        g_inheritance_high_operations++;
        mutex_unlock(&shared_mutex);
        sleep_ticks(1U);
    }
}

static void medium_task(void *argument)
{
    (void)argument;
    while (1)
    {
        g_inheritance_medium_operations++;
    }
}

static const task_definition_t inheritance_tasks[] = {
    { low_owner_task, 0U, KERNEL_TASK_STACK_WORDS, LOW_BASE_PRIORITY, "low-owner", 0U },
    { high_waiter_task, 0U, KERNEL_TASK_STACK_WORDS, HIGH_PRIORITY, "high-waiter", 0U },
    { medium_task, 0U, KERNEL_TASK_STACK_WORDS, MEDIUM_PRIORITY, "medium", 0U }
};

void mutex_priority_inheritance_start(void)
{
    const kernel_config_t config = {
        inheritance_tasks,
        sizeof(inheritance_tasks) / sizeof(inheritance_tasks[0])
    };

    mutex_init(&shared_mutex);
    g_inheritance_low_priority = 0U;
    g_inheritance_high_state = 0U;
    g_inheritance_low_operations = 0U;
    g_inheritance_high_operations = 0U;
    g_inheritance_medium_operations = 0U;
    g_inheritance_error = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
