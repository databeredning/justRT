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

static JRT_Mutex_t first_mutex;
static JRT_Mutex_t second_mutex;
static JRT_Semaphore_t bridge_start_gate;
static JRT_Semaphore_t high_start_gate;
static JRT_Semaphore_t non_owner_start_gate;

mutex_test_state_t g_test_mutex;

static void test_mutex_owner_task(void *argument)
{
    JRT_TaskState_t bridge_state;
    JRT_TaskState_t high_state;
    uint32_t priority;

    (void)argument;
    g_test_mutex.result.state = TEST_STATE_RUNNING;

    g_test_mutex.recursive_lock_first =
        (uint32_t)JRT_MutexLock(&first_mutex, JRT_WAIT_FOREVER);
    g_test_mutex.recursive_lock_second =
        (uint32_t)JRT_MutexLock(&first_mutex, JRT_WAIT_FOREVER);
    g_test_mutex.recursive_unlock_first = (uint32_t)JRT_MutexUnlock(&first_mutex);
    if (g_test_mutex.recursive_lock_first != 1U
        || g_test_mutex.recursive_lock_second != 1U
        || g_test_mutex.recursive_unlock_first != 1U)
    {
        g_test_mutex.error_code = 1U;
    }

    JRT_SemaphoreGive(&bridge_start_gate);
    JRT_SemaphoreGive(&non_owner_start_gate);
    while (1)
    {
        if (JRT_TaskGetState(BRIDGE_TASK_ID, &bridge_state) != JRT_STATUS_OK)
        {
            g_test_mutex.error_code = 2U;
            break;
        }
        if (bridge_state == JRT_TASK_STATE_BLOCKED)
        {
            g_test_mutex.bridge_blocked = 1U;
            break;
        }
        JRT_TaskDelay(1U);
    }

    JRT_SemaphoreGive(&high_start_gate);
    while (1)
    {
        if (JRT_TaskGetState(HIGH_TASK_ID, &high_state) != JRT_STATUS_OK)
        {
            g_test_mutex.error_code = 2U;
            break;
        }
        if (high_state == JRT_TASK_STATE_BLOCKED)
        {
            g_test_mutex.high_blocked = 1U;
            break;
        }
        JRT_TaskDelay(1U);
    }

    if (JRT_TaskGetPriority(OWNER_TASK_ID, &priority) != JRT_STATUS_OK)
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

    g_test_mutex.recursive_unlock_second = (uint32_t)JRT_MutexUnlock(&first_mutex);
    if (g_test_mutex.recursive_unlock_second == 0U)
    {
        g_test_mutex.error_code = 5U;
    }

    JRT_TaskDelay(2U);
    if (g_test_mutex.recursive_unlock_second != 1U
        || g_test_mutex.bridge_acquired_first == 0U
        || g_test_mutex.high_acquired_second == 0U)
    {
        g_test_mutex.error_code = 6U;
    }
    if (JRT_TaskGetPriority(OWNER_TASK_ID, &priority) != JRT_STATUS_OK
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
    g_test_mutex.result.state = TEST_STATE_COMPLETE;
    g_test_mutex.result.done = 1U;

    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

static void test_mutex_bridge_task(void *argument)
{
    (void)argument;
    JRT_SemaphoreTake(&bridge_start_gate, JRT_WAIT_FOREVER);

    if (JRT_MutexLock(&second_mutex, JRT_WAIT_FOREVER) == 0
        || JRT_MutexLock(&first_mutex, JRT_WAIT_FOREVER) == 0)
    {
        g_test_mutex.error_code = 8U;
    }
    else
    {
        g_test_mutex.bridge_acquired_first = 1U;
        JRT_MutexUnlock(&first_mutex);
        JRT_MutexUnlock(&second_mutex);
    }

    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

static void test_mutex_high_task(void *argument)
{
    (void)argument;
    JRT_SemaphoreTake(&high_start_gate, JRT_WAIT_FOREVER);

    if (JRT_MutexLock(&second_mutex, JRT_WAIT_FOREVER) == 0)
    {
        g_test_mutex.error_code = 9U;
    }
    else
    {
        g_test_mutex.high_acquired_second = 1U;
        JRT_MutexUnlock(&second_mutex);
    }

    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

static void test_mutex_non_owner_task(void *argument)
{
    (void)argument;
    JRT_SemaphoreTake(&non_owner_start_gate, JRT_WAIT_FOREVER);
    g_test_mutex.non_owner_unlock = (uint32_t)JRT_MutexUnlock(&first_mutex);
    if (g_test_mutex.non_owner_unlock != 0U)
    {
        g_test_mutex.error_code = 10U;
    }

    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

JRT_DECLARE_STATIC_TASK_STACK(test_mutex_owner_stack, JRT_TASK_STACK_WORDS);
JRT_DECLARE_STATIC_TASK_STACK(test_mutex_bridge_stack, JRT_TASK_STACK_WORDS);
JRT_DECLARE_STATIC_TASK_STACK(test_mutex_high_stack, JRT_TASK_STACK_WORDS);
JRT_DECLARE_STATIC_TASK_STACK(test_mutex_non_owner_stack, JRT_TASK_STACK_WORDS);

static const JRT_TaskDefinition_t test_mutex_tasks[] = {
    JRT_TASK_DEFINITION(test_mutex_owner_task, 0U, test_mutex_owner_stack, OWNER_PRIORITY, "test-mutex-owner", 0U),
    JRT_TASK_DEFINITION(test_mutex_bridge_task, 0U, test_mutex_bridge_stack, BRIDGE_PRIORITY, "test-mutex-bridge", 0U),
    JRT_TASK_DEFINITION(test_mutex_high_task, 0U, test_mutex_high_stack, HIGH_PRIORITY, "test-mutex-high", 0U),
    JRT_TASK_DEFINITION(test_mutex_non_owner_task, 0U, test_mutex_non_owner_stack, NON_OWNER_PRIORITY, "test-mutex-non-owner", 0U)
};

void test_mutex_start(void)
{
    const JRT_KernelConfig_t config = {
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

    JRT_MutexCreateRecursiveStatic(&first_mutex);
    JRT_MutexCreateRecursiveStatic(&second_mutex);
    JRT_SemaphoreCreateBinaryStatic(&bridge_start_gate, 0U);
    JRT_SemaphoreCreateBinaryStatic(&high_start_gate, 0U);
    JRT_SemaphoreCreateBinaryStatic(&non_owner_start_gate, 0U);
    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_mutex.result.fail = 1U;
        return;
    }
    JRT_KernelStart();
}
