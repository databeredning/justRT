#include "kernel.h"
#include "sync.h"
#include "test_mutex.h"

#define OWNER_TASK_ID 0U
#define BRIDGE_TASK_ID 1U
#define HIGH_TASK_ID 2U
#define NON_OWNER_TASK_ID 3U

#define OWNER_PRIORITY 1U
#define BRIDGE_PRIORITY 2U
#define HIGH_PRIORITY 3U
#define NON_OWNER_PRIORITY 0U

static mutex_t first_mutex;
static mutex_t second_mutex;

mutex_test_state_t g_test_mutex;

static void test_mutex_owner_task(void *argument)
{
    task_state_t bridge_state;
    task_state_t high_state;
    uint32_t priority;

    (void)argument;
    g_test_mutex.result.state = TEST_STATE_RUNNING;

    g_test_mutex.recursive_lock_first =
        (uint32_t)mutex_lock(&first_mutex, SEMAPHORE_WAIT_FOREVER);
    g_test_mutex.recursive_lock_second =
        (uint32_t)mutex_lock(&first_mutex, SEMAPHORE_WAIT_FOREVER);
    g_test_mutex.recursive_unlock_first = (uint32_t)mutex_unlock(&first_mutex);
    if (g_test_mutex.recursive_lock_first != 1U
        || g_test_mutex.recursive_lock_second != 1U
        || g_test_mutex.recursive_unlock_first != 1U)
    {
        g_test_mutex.error_code = 1U;
    }

    while (1)
    {
        if (task_get_state(BRIDGE_TASK_ID, &bridge_state) != KERNEL_OK
            || task_get_state(HIGH_TASK_ID, &high_state) != KERNEL_OK)
        {
            g_test_mutex.error_code = 2U;
            break;
        }
        if (bridge_state == TASK_STATE_BLOCKED
            && high_state == TASK_STATE_BLOCKED)
        {
            g_test_mutex.bridge_blocked = 1U;
            g_test_mutex.high_blocked = 1U;
            break;
        }
        sleep_ticks(1U);
    }

    if (task_get_priority(OWNER_TASK_ID, &priority) != KERNEL_OK)
    {
        g_test_mutex.error_code = 3U;
    }
    else
    {
        g_test_mutex.owner_priority_full_chain = priority;
        if (priority != HIGH_PRIORITY)
        {
            g_test_mutex.error_code = 4U;
        }
    }

    g_test_mutex.recursive_unlock_second = (uint32_t)mutex_unlock(&first_mutex);
    if (g_test_mutex.recursive_unlock_second == 0U)
    {
        g_test_mutex.error_code = 5U;
    }

    sleep_ticks(2U);
    if (g_test_mutex.recursive_unlock_second != 1U
        || g_test_mutex.bridge_acquired_first == 0U
        || g_test_mutex.high_acquired_second == 0U)
    {
        g_test_mutex.error_code = 6U;
    }
    if (task_get_priority(OWNER_TASK_ID, &priority) != KERNEL_OK
        || priority != OWNER_PRIORITY)
    {
        g_test_mutex.error_code = 7U;
    }

    if (g_test_mutex.error_code == 0U)
    {
        g_test_mutex.result.pass = 1U;
    }
    else
    {
        g_test_mutex.result.fail = 1U;
    }
    g_test_mutex.result.done = 1U;
    g_test_mutex.result.state = TEST_STATE_COMPLETE;

    while (1)
    {
        sleep_ticks(1U);
    }
}

static void test_mutex_bridge_task(void *argument)
{
    (void)argument;
    sleep_ticks(1U);

    if (mutex_lock(&second_mutex, SEMAPHORE_WAIT_FOREVER) == 0
        || mutex_lock(&first_mutex, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_test_mutex.error_code = 8U;
    }
    else
    {
        g_test_mutex.bridge_acquired_first = 1U;
        mutex_unlock(&first_mutex);
        mutex_unlock(&second_mutex);
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static void test_mutex_high_task(void *argument)
{
    (void)argument;
    sleep_ticks(3U);

    if (mutex_lock(&second_mutex, SEMAPHORE_WAIT_FOREVER) == 0)
    {
        g_test_mutex.error_code = 9U;
    }
    else
    {
        g_test_mutex.high_acquired_second = 1U;
        mutex_unlock(&second_mutex);
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static void test_mutex_non_owner_task(void *argument)
{
    (void)argument;
    sleep_ticks(2U);
    g_test_mutex.non_owner_unlock = (uint32_t)mutex_unlock(&first_mutex);
    if (g_test_mutex.non_owner_unlock != 0U)
    {
        g_test_mutex.error_code = 10U;
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static const task_definition_t test_mutex_tasks[] = {
    { test_mutex_owner_task, 0U, KERNEL_TASK_STACK_WORDS, OWNER_PRIORITY,
        "test-mutex-owner", 0U },
    { test_mutex_bridge_task, 0U, KERNEL_TASK_STACK_WORDS, BRIDGE_PRIORITY,
        "test-mutex-bridge", 0U },
    { test_mutex_high_task, 0U, KERNEL_TASK_STACK_WORDS, HIGH_PRIORITY,
        "test-mutex-high", 0U },
    { test_mutex_non_owner_task, 0U, KERNEL_TASK_STACK_WORDS,
        NON_OWNER_PRIORITY, "test-mutex-non-owner", 0U }
};

void test_mutex_start(void)
{
    const kernel_config_t config = {
        test_mutex_tasks,
        sizeof(test_mutex_tasks) / sizeof(test_mutex_tasks[0])
    };

    g_test_mutex.result.state = TEST_STATE_IDLE;
    g_test_mutex.result.runs = 0U;
    g_test_mutex.result.pass = 0U;
    g_test_mutex.result.fail = 0U;
    g_test_mutex.result.done = 0U;
    g_test_mutex.recursive_lock_first = 0U;
    g_test_mutex.recursive_lock_second = 0U;
    g_test_mutex.recursive_unlock_first = 0U;
    g_test_mutex.recursive_unlock_second = 0U;
    g_test_mutex.non_owner_unlock = 0U;
    g_test_mutex.owner_priority_full_chain = 0U;
    g_test_mutex.bridge_blocked = 0U;
    g_test_mutex.high_blocked = 0U;
    g_test_mutex.bridge_acquired_first = 0U;
    g_test_mutex.high_acquired_second = 0U;
    g_test_mutex.error_code = 0U;

    mutex_init(&first_mutex);
    mutex_init(&second_mutex);
    if (kernel_init(&config) != KERNEL_OK)
    {
        g_test_mutex.result.fail = 1U;
        return;
    }
    kernel_start();
}
