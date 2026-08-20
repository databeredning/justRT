#include <stdint.h>

#include "kernel.h"
#include "mutex_multi_restore.h"

#define OWNER_TASK_ID 0U
#define HIGH_WAITER_TASK_ID 1U
#define MID_WAITER_TASK_ID 2U

#define OWNER_BASE_PRIORITY 1U
#define MID_WAITER_PRIORITY 2U
#define HIGH_WAITER_PRIORITY 3U

static mutex_t mutex_high;
static mutex_t mutex_mid;

volatile uint32_t g_multi_restore_owner_priority_before_release;
volatile uint32_t g_multi_restore_owner_priority_after_first_release;
volatile uint32_t g_multi_restore_owner_priority_after_second_release;
volatile uint32_t g_multi_restore_high_waiter_acquired;
volatile uint32_t g_multi_restore_mid_waiter_acquired;
volatile uint32_t g_multi_restore_error;
volatile uint32_t g_multi_restore_done;

static void owner_task(void *argument)
{
    uint32_t priority;
    task_state_t high_state;
    task_state_t mid_state;

    (void)argument;

    if (mutex_lock(&mutex_high, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_multi_restore_error = 1U;
    }
    if (mutex_lock(&mutex_mid, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_multi_restore_error = 2U;
    }

    sleep_ticks(1U);

    while (1)
    {
        if ((task_get_state(HIGH_WAITER_TASK_ID, &high_state) != KERNEL_OK)
            || (task_get_state(MID_WAITER_TASK_ID, &mid_state) != KERNEL_OK))
        {
            g_multi_restore_error = 3U;
            break;
        }
        if ((high_state == TASK_STATE_BLOCKED) && (mid_state == TASK_STATE_BLOCKED))
        {
            break;
        }
        sleep_ticks(1U);
    }

    if (task_get_priority(OWNER_TASK_ID, &priority) != KERNEL_OK)
    {
        g_multi_restore_error = 4U;
    }
    else
    {
        g_multi_restore_owner_priority_before_release = priority;
        if (priority != HIGH_WAITER_PRIORITY)
        {
            g_multi_restore_error = 5U;
        }
    }

    if (mutex_unlock(&mutex_high) == 0)
    {
        g_multi_restore_error = 6U;
    }

    if (task_get_priority(OWNER_TASK_ID, &priority) != KERNEL_OK)
    {
        g_multi_restore_error = 7U;
    }
    else
    {
        g_multi_restore_owner_priority_after_first_release = priority;
        if (priority != MID_WAITER_PRIORITY)
        {
            g_multi_restore_error = 8U;
        }
    }

    if (mutex_unlock(&mutex_mid) == 0)
    {
        g_multi_restore_error = 9U;
    }

    if (task_get_priority(OWNER_TASK_ID, &priority) != KERNEL_OK)
    {
        g_multi_restore_error = 10U;
    }
    else
    {
        g_multi_restore_owner_priority_after_second_release = priority;
        if (priority != OWNER_BASE_PRIORITY)
        {
            g_multi_restore_error = 11U;
        }
    }

    sleep_ticks(1U);

    if ((g_multi_restore_high_waiter_acquired == 0U)
        || (g_multi_restore_mid_waiter_acquired == 0U))
    {
        g_multi_restore_error = 12U;
    }

    g_multi_restore_done = 1U;

    while (1)
    {
        sleep_ticks(1U);
    }
}

static void high_waiter_task(void *argument)
{
    (void)argument;

    if (mutex_lock(&mutex_high, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_multi_restore_error = 13U;
    }
    else
    {
        g_multi_restore_high_waiter_acquired = 1U;
        mutex_unlock(&mutex_high);
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static void mid_waiter_task(void *argument)
{
    (void)argument;

    if (mutex_lock(&mutex_mid, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_multi_restore_error = 14U;
    }
    else
    {
        g_multi_restore_mid_waiter_acquired = 1U;
        mutex_unlock(&mutex_mid);
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static const task_definition_t multi_restore_tasks[] = {
    { owner_task, 0U, KERNEL_TASK_STACK_WORDS, OWNER_BASE_PRIORITY, "owner", 0U },
    { high_waiter_task, 0U, KERNEL_TASK_STACK_WORDS, HIGH_WAITER_PRIORITY, "high-waiter", 0U },
    { mid_waiter_task, 0U, KERNEL_TASK_STACK_WORDS, MID_WAITER_PRIORITY, "mid-waiter", 0U }
};

void mutex_multi_restore_start(void)
{
    const kernel_config_t config = {
        multi_restore_tasks,
        sizeof(multi_restore_tasks) / sizeof(multi_restore_tasks[0])
    };

    mutex_init(&mutex_high);
    mutex_init(&mutex_mid);
    g_multi_restore_owner_priority_before_release = 0U;
    g_multi_restore_owner_priority_after_first_release = 0U;
    g_multi_restore_owner_priority_after_second_release = 0U;
    g_multi_restore_high_waiter_acquired = 0U;
    g_multi_restore_mid_waiter_acquired = 0U;
    g_multi_restore_error = 0U;
    g_multi_restore_done = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
