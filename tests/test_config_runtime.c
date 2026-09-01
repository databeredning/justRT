#include "kernel.h"
#include "test_config_runtime.h"

config_runtime_test_state_t g_test_config_runtime JRT_TASK_UNPRIVILEGED_DATA;

static void config_runtime_task(void *argument)
{
    (void)argument;

    if ((JRT_MillisecondsToTicks(0U) == 0U)
        && (JRT_MillisecondsToTicks(1U) == 2U)
        && (JRT_MillisecondsToTicks(999U) == 1023U)
        && (JRT_MillisecondsToTicks(1000U) == 1024U))
    {
        g_test_config_runtime.conversion_checks = 4U;
    }
    if (JRT_MillisecondsToTicks(UINT32_MAX) == UINT32_MAX)
    {
        g_test_config_runtime.saturation_checked = 1U;
    }
    if ((g_test_config_runtime.priority_rejected != 0U)
        && (g_test_config_runtime.conversion_checks == 4U)
        && (g_test_config_runtime.saturation_checked != 0U))
    {
        g_test_config_runtime.result.pass = 1U;
    }
    else
    {
        g_test_config_runtime.error_code = 1U;
        g_test_config_runtime.result.fail = 1U;
    }
    g_test_config_runtime.result.runs = 1U;
    g_test_config_runtime.result.state = TEST_STATE_COMPLETE;
    g_test_config_runtime.result.done = 1U;
    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

JRT_DECLARE_STATIC_TASK_STACK(config_runtime_stack, JRT_TASK_STACK_WORDS);

static const JRT_TaskDefinition_t config_runtime_tasks[] = {
    JRT_TASK_DEFINITION(config_runtime_task, 0U, config_runtime_stack, 1U, "config-runtime", 0U)
};

void test_config_runtime_start(void)
{
    JRT_TaskDefinition_t invalid_task = config_runtime_tasks[0];
    const JRT_KernelConfig_t invalid_config = { &invalid_task, 1U };
    const JRT_KernelConfig_t valid_config = {
        config_runtime_tasks,
        sizeof(config_runtime_tasks) / sizeof(config_runtime_tasks[0])
    };

    invalid_task.priority = JRT_MAX_TASK_PRIORITY + 1U;
    if (JRT_KernelInit(&invalid_config) == JRT_STATUS_INVALID_PRIORITY)
    {
        g_test_config_runtime.priority_rejected = 1U;
    }
    if (JRT_KernelInit(&valid_config) != JRT_STATUS_OK)
    {
        g_test_config_runtime.error_code = 2U;
        g_test_config_runtime.result.fail = 1U;
        g_test_config_runtime.result.done = 1U;
        return;
    }
    g_test_config_runtime.result.state = TEST_STATE_RUNNING;
    JRT_KernelStart();
}
