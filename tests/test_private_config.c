#include "kernel.h"
#include "test_private_config.h"

#define PRIVATE_CONFIG_REGION_SIZE 32U
#define PRIVATE_CONFIG_INVALID_CASES 8U

typedef struct
{
    volatile uint32_t words[PRIVATE_CONFIG_REGION_SIZE / sizeof(uint32_t)];
} private_config_region_t;

static private_config_region_t private_config_region_a
    JRT_TASK_PRIVATE_DATA(PRIVATE_CONFIG_REGION_SIZE);
static private_config_region_t private_config_region_b
    JRT_TASK_PRIVATE_DATA(PRIVATE_CONFIG_REGION_SIZE);

private_config_test_state_t g_test_private_config JRT_TASK_UNPRIVILEGED_DATA;

static JRT_TASK_UNPRIVILEGED void private_config_task_a(void *argument)
{
    private_config_region_t *region = (private_config_region_t *)argument;

    region->words[0] = 0xA11A11A1U;
    g_test_private_config.task_a_value = region->words[0];
    while (1)
    {
        if (g_test_private_config.task_b_value == 0xB22B22B2U)
        {
            g_test_private_config.tasks_ran = 2U;
            if ((g_test_private_config.task_a_value == 0xA11A11A1U)
                && (g_test_private_config.task_b_value == 0xB22B22B2U)
                && (g_test_private_config.invalid_cases_rejected
                    == PRIVATE_CONFIG_INVALID_CASES))
            {
                g_test_private_config.result.pass = 1U;
            }
            else
            {
                g_test_private_config.error_code = 20U;
                g_test_private_config.result.fail = 1U;
            }
            g_test_private_config.result.runs = 2U;
            g_test_private_config.result.state = TEST_STATE_COMPLETE;
            g_test_private_config.result.done = 1U;
        }
        JRT_TaskDelay(1U);
    }
}

static JRT_TASK_UNPRIVILEGED void private_config_task_b(void *argument)
{
    private_config_region_t *region = (private_config_region_t *)argument;

    region->words[0] = 0xB22B22B2U;
    g_test_private_config.task_b_value = region->words[0];
    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

JRT_DECLARE_STATIC_TASK_STACK(private_config_stack_a, JRT_TASK_STACK_WORDS);
JRT_DECLARE_STATIC_TASK_STACK(private_config_stack_b, JRT_TASK_STACK_WORDS);

static const JRT_TaskDefinition_t private_config_valid_tasks[] = {
    JRT_TASK_DEFINITION_WITH_PRIVATE_DATA(
        private_config_task_a, &private_config_region_a, private_config_stack_a,
        1U, "private-config-a", JRT_TASK_FLAG_UNPRIVILEGED,
        private_config_region_a),
    JRT_TASK_DEFINITION_WITH_PRIVATE_DATA(
        private_config_task_b, &private_config_region_b, private_config_stack_b,
        1U, "private-config-b", JRT_TASK_FLAG_UNPRIVILEGED,
        private_config_region_b)
};

static int expect_invalid(JRT_TaskDefinition_t *tasks)
{
    const JRT_KernelConfig_t config = { tasks, 2U };

    if (JRT_KernelInit(&config) == JRT_STATUS_INVALID_MEMORY_REGION)
    {
        g_test_private_config.invalid_cases_rejected++;
        return 1;
    }
    return 0;
}

void test_private_config_start(void)
{
    JRT_TaskDefinition_t tasks[2];
    const JRT_KernelConfig_t valid_config = { tasks, 2U };

    g_test_private_config.result.state = TEST_STATE_IDLE;
    g_test_private_config.result.runs = 0U;
    g_test_private_config.result.pass = 0U;
    g_test_private_config.result.fail = 0U;
    g_test_private_config.result.done = 0U;
    g_test_private_config.invalid_cases_rejected = 0U;
    g_test_private_config.valid_config_accepted = 0U;
    g_test_private_config.tasks_ran = 0U;
    g_test_private_config.task_a_value = 0U;
    g_test_private_config.task_b_value = 0U;
    g_test_private_config.error_code = 0U;

    tasks[0] = private_config_valid_tasks[0];
    tasks[1] = private_config_valid_tasks[1];
    tasks[0].private_data_base = 0U;
    if (expect_invalid(tasks) == 0)
    {
        g_test_private_config.error_code = 1U;
    }

    tasks[0] = private_config_valid_tasks[0];
    tasks[0].private_data_size = 16U;
    if (expect_invalid(tasks) == 0)
    {
        g_test_private_config.error_code = 2U;
    }

    tasks[0] = private_config_valid_tasks[0];
    tasks[0].private_data_size = 48U;
    if (expect_invalid(tasks) == 0)
    {
        g_test_private_config.error_code = 3U;
    }

    tasks[0] = private_config_valid_tasks[0];
    tasks[0].private_data_base = (void *)((uintptr_t)&private_config_region_a
                                          + sizeof(uint32_t));
    if (expect_invalid(tasks) == 0)
    {
        g_test_private_config.error_code = 4U;
    }

    tasks[0] = private_config_valid_tasks[0];
    tasks[0].private_data_base =
        JRT_TASK_STACK_GUARD(private_config_stack_a);
    if (expect_invalid(tasks) == 0)
    {
        g_test_private_config.error_code = 5U;
    }

    tasks[0] = private_config_valid_tasks[0];
    tasks[1] = private_config_valid_tasks[1];
    tasks[1].private_data_base = tasks[0].private_data_base;
    if (expect_invalid(tasks) == 0)
    {
        g_test_private_config.error_code = 6U;
    }

    tasks[0] = private_config_valid_tasks[0];
    tasks[1] = private_config_valid_tasks[1];
    tasks[0].flags = 0U;
    if (expect_invalid(tasks) == 0)
    {
        g_test_private_config.error_code = 7U;
    }

    tasks[0] = private_config_valid_tasks[0];
    tasks[0].private_data_base = (void *)(uintptr_t)32U;
    if (expect_invalid(tasks) == 0)
    {
        g_test_private_config.error_code = 8U;
    }

    tasks[0] = private_config_valid_tasks[0];
    tasks[1] = private_config_valid_tasks[1];
    if (g_test_private_config.error_code != 0U)
    {
        g_test_private_config.result.fail = 1U;
        return;
    }
    if (JRT_KernelInit(&valid_config) != JRT_STATUS_OK)
    {
        g_test_private_config.error_code = 9U;
        g_test_private_config.result.fail = 1U;
        return;
    }
    g_test_private_config.valid_config_accepted = 1U;
    g_test_private_config.result.state = TEST_STATE_RUNNING;
    JRT_KernelStart();
}
