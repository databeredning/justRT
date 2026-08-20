#include <stdint.h>

#include "kernel.h"
#include "mutex_chain_inheritance.h"

#define OWNER_TASK_ID 0U
#define BRIDGE_TASK_ID 1U
#define HIGH_TASK_ID 2U

#define OWNER_PRIORITY 1U
#define BRIDGE_PRIORITY 2U
#define HIGH_PRIORITY 3U

#define HIGH_TASK_START_DELAY_TICKS 3U

static mutex_t mutex_1;
static mutex_t mutex_2;

volatile uint32_t g_chain_owner_priority_after_chain;
volatile uint32_t g_chain_bridge_blocked;
volatile uint32_t g_chain_high_blocked;
volatile uint32_t g_chain_bridge_acquired_mutex_1;
volatile uint32_t g_chain_high_acquired_mutex_2;
volatile uint32_t g_chain_error;
volatile uint32_t g_chain_done;

static TASK_UNPRIVILEGED void owner_task(void *argument)
{
    task_state_t bridge_state;
    task_state_t high_state;
    uint32_t owner_priority;

    (void)argument;

    if (mutex_lock(&mutex_1, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_chain_error = 1U;
    }

    while (1)
    {
        if ((task_get_state(BRIDGE_TASK_ID, &bridge_state) != KERNEL_OK)
            || (task_get_state(HIGH_TASK_ID, &high_state) != KERNEL_OK))
        {
            g_chain_error = 2U;
            break;
        }

        if ((bridge_state == TASK_STATE_BLOCKED) && (high_state == TASK_STATE_BLOCKED))
        {
            g_chain_bridge_blocked = 1U;
            g_chain_high_blocked = 1U;
            break;
        }

        sleep_ticks(1U);
    }

    if (task_get_priority(OWNER_TASK_ID, &owner_priority) != KERNEL_OK)
    {
        g_chain_error = 3U;
    }
    else
    {
        g_chain_owner_priority_after_chain = owner_priority;
        if (owner_priority != HIGH_PRIORITY)
        {
            g_chain_error = 4U;
        }
    }

    if (mutex_unlock(&mutex_1) == 0)
    {
        g_chain_error = 5U;
    }

    sleep_ticks(2U);

    if ((g_chain_bridge_acquired_mutex_1 == 0U)
        || (g_chain_high_acquired_mutex_2 == 0U))
    {
        g_chain_error = 6U;
    }

    g_chain_done = 1U;

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
        g_chain_error = 7U;
    }

    sleep_ticks(1U);

    if (mutex_lock(&mutex_1, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_chain_error = 8U;
    }
    else
    {
        g_chain_bridge_acquired_mutex_1 = 1U;
        mutex_unlock(&mutex_1);
        mutex_unlock(&mutex_2);
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void high_task(void *argument)
{
    (void)argument;

    sleep_ticks(HIGH_TASK_START_DELAY_TICKS);

    if (mutex_lock(&mutex_2, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_chain_error = 9U;
    }
    else
    {
        g_chain_high_acquired_mutex_2 = 1U;
        mutex_unlock(&mutex_2);
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static const task_definition_t chain_tasks[] = {
    { owner_task, 0U, KERNEL_TASK_STACK_WORDS, OWNER_PRIORITY, "chain-owner", 0U },
    { bridge_task, 0U, KERNEL_TASK_STACK_WORDS, BRIDGE_PRIORITY, "chain-bridge", 0U },
    { high_task, 0U, KERNEL_TASK_STACK_WORDS, HIGH_PRIORITY, "chain-high", 0U }
};

void mutex_chain_inheritance_start(void)
{
    const kernel_config_t config = {
        chain_tasks,
        sizeof(chain_tasks) / sizeof(chain_tasks[0])
    };

    mutex_init(&mutex_1);
    mutex_init(&mutex_2);
    g_chain_owner_priority_after_chain = 0U;
    g_chain_bridge_blocked = 0U;
    g_chain_high_blocked = 0U;
    g_chain_bridge_acquired_mutex_1 = 0U;
    g_chain_high_acquired_mutex_2 = 0U;
    g_chain_error = 0U;
    g_chain_done = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
