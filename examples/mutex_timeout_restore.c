#include <stdint.h>

#include "kernel.h"
#include "mutex_timeout_restore.h"

#define OWNER_TASK_ID 0U
#define BRIDGE_TASK_ID 1U
#define HIGH_TASK_ID 2U

#define OWNER_PRIORITY 1U
#define BRIDGE_PRIORITY 2U
#define HIGH_PRIORITY 3U

#define HIGH_TIMEOUT_TICKS 4U

static mutex_t mutex_1;
static mutex_t mutex_2;

volatile uint32_t g_timeout_restore_owner_priority_full_chain;
volatile uint32_t g_timeout_restore_owner_priority_after_timeout;
volatile uint32_t g_timeout_restore_bridge_still_blocked;
volatile uint32_t g_timeout_restore_error;
volatile uint32_t g_timeout_restore_done;

static TASK_UNPRIVILEGED void owner_task(void *argument)
{
    task_state_t bridge_state;
    task_state_t high_state;
    uint32_t priority;

    (void)argument;

    if (mutex_lock(&mutex_1, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_timeout_restore_error = 1U;
    }

    while (1)
    {
        if ((task_get_state(BRIDGE_TASK_ID, &bridge_state) != KERNEL_OK)
            || (task_get_state(HIGH_TASK_ID, &high_state) != KERNEL_OK))
        {
            g_timeout_restore_error = 2U;
            break;
        }
        if ((bridge_state == TASK_STATE_BLOCKED) && (high_state == TASK_STATE_BLOCKED))
        {
            break;
        }
        sleep_ticks(1U);
    }

    if (task_get_priority(OWNER_TASK_ID, &priority) != KERNEL_OK)
    {
        g_timeout_restore_error = 3U;
    }
    else
    {
        g_timeout_restore_owner_priority_full_chain = priority;
        if (priority != HIGH_PRIORITY)
        {
            g_timeout_restore_error = 4U;
        }
    }

    sleep_ticks(HIGH_TIMEOUT_TICKS + 2U);

    if (task_get_state(HIGH_TASK_ID, &high_state) != KERNEL_OK)
    {
        g_timeout_restore_error = 5U;
    }
    else if (high_state == TASK_STATE_BLOCKED)
    {
        g_timeout_restore_error = 6U;
    }

    if (task_get_state(BRIDGE_TASK_ID, &bridge_state) != KERNEL_OK)
    {
        g_timeout_restore_error = 7U;
    }
    else if (bridge_state == TASK_STATE_BLOCKED)
    {
        g_timeout_restore_bridge_still_blocked = 1U;
    }

    if (task_get_priority(OWNER_TASK_ID, &priority) != KERNEL_OK)
    {
        g_timeout_restore_error = 8U;
    }
    else
    {
        g_timeout_restore_owner_priority_after_timeout = priority;
        if (priority != BRIDGE_PRIORITY)
        {
            g_timeout_restore_error = 9U;
        }
    }

    g_timeout_restore_done = 1U;

    while (1)
    {
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void bridge_task(void *argument)
{
    (void)argument;

    if (mutex_lock(&mutex_2, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_timeout_restore_error = 10U;
    }

    sleep_ticks(1U);

    if (mutex_lock(&mutex_1, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_timeout_restore_error = 11U;
    }
    else
    {
        mutex_unlock(&mutex_1);
        mutex_unlock(&mutex_2);
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void high_waiter_task(void *argument)
{
    (void)argument;

    sleep_ticks(2U);

    if (mutex_lock(&mutex_2, HIGH_TIMEOUT_TICKS) != 0)
    {
        g_timeout_restore_error = 12U;
        mutex_unlock(&mutex_2);
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static const task_definition_t timeout_restore_tasks[] = {
    { owner_task, 0U, KERNEL_TASK_STACK_WORDS, OWNER_PRIORITY, "to-owner", 0U },
    { bridge_task, 0U, KERNEL_TASK_STACK_WORDS, BRIDGE_PRIORITY, "to-bridge", 0U },
    { high_waiter_task, 0U, KERNEL_TASK_STACK_WORDS, HIGH_PRIORITY, "to-high", 0U }
};

void mutex_timeout_restore_start(void)
{
    const kernel_config_t config = {
        timeout_restore_tasks,
        sizeof(timeout_restore_tasks) / sizeof(timeout_restore_tasks[0])
    };

    mutex_init(&mutex_1);
    mutex_init(&mutex_2);
    g_timeout_restore_owner_priority_full_chain = 0U;
    g_timeout_restore_owner_priority_after_timeout = 0U;
    g_timeout_restore_bridge_still_blocked = 0U;
    g_timeout_restore_error = 0U;
    g_timeout_restore_done = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
