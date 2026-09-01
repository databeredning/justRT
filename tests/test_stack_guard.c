#include "kernel.h"
#include "test_stack_guard.h"

stack_guard_test_state_t g_test_stack_guard JRT_TASK_UNPRIVILEGED_DATA;

JRT_DECLARE_STATIC_TASK_STACK(test_stack_guard_stack, JRT_TASK_STACK_WORDS);

static JRT_TASK_UNPRIVILEGED void test_stack_guard_task(void *argument)
{
    volatile uint32_t *guard = (volatile uint32_t *)argument;

    g_test_stack_guard.result.state = TEST_STATE_RUNNING;
    g_test_stack_guard.write_attempted = 1U;
    *guard = 0xBAD00BADU;

    g_test_stack_guard.result.fail = 1U;
    g_test_stack_guard.result.state = TEST_STATE_COMPLETE;
    g_test_stack_guard.result.done = 1U;
    while (1)
    {
    }
}

static const JRT_TaskDefinition_t test_stack_guard_tasks[] JRT_TASK_UNPRIVILEGED_RODATA = {
    JRT_TASK_DEFINITION(test_stack_guard_task,
                        JRT_TASK_STACK_GUARD(test_stack_guard_stack),
                        test_stack_guard_stack, 1U, "test-stack-guard",
                        JRT_TASK_FLAG_UNPRIVILEGED)
};

void test_stack_guard_start(void)
{
    const JRT_KernelConfig_t config = {
        test_stack_guard_tasks,
        sizeof(test_stack_guard_tasks) / sizeof(test_stack_guard_tasks[0])
    };

    g_test_stack_guard.result.state = TEST_STATE_IDLE;
    g_test_stack_guard.result.runs = 0U;
    g_test_stack_guard.result.pass = 0U;
    g_test_stack_guard.result.fail = 0U;
    g_test_stack_guard.result.done = 0U;
    g_test_stack_guard.expected_guard_address =
        (uint32_t)(uintptr_t)JRT_TASK_STACK_GUARD(test_stack_guard_stack);
    g_test_stack_guard.write_attempted = 0U;

    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_stack_guard.result.fail = 1U;
        return;
    }
    JRT_KernelStart();
}
