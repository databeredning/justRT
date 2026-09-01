#include "kernel.h"
#include "test_fatal_hook.h"

fatal_hook_test_state_t g_test_fatal_hook KERNEL_PRIVILEGED_DATA;

#if defined(JUSTRT_TEST_FATAL_HOOK)

void JRT_FatalErrorHook(JRT_FatalReason_t reason)
{
    uint32_t primask;

    __asm volatile ("mrs %0, primask" : "=r" (primask) : : "memory");
    g_test_fatal_hook.hook_calls++;
    g_test_fatal_hook.hook_reason = (uint32_t)reason;
    g_test_fatal_hook.interrupts_masked = primask & 1U;
    if ((reason != JRT_FATAL_KERNEL_INVARIANT)
        || (g_fatal_active == 0U)
        || (g_fatal_reason != JRT_FATAL_KERNEL_INVARIANT)
        || (g_test_fatal_hook.interrupts_masked == 0U))
    {
        g_test_fatal_hook.error_code = 1U;
        g_test_fatal_hook.result.fail = 1U;
    }
    else
    {
        g_test_fatal_hook.result.pass = 1U;
    }
    g_test_fatal_hook.result.runs = 1U;
    g_test_fatal_hook.result.state = TEST_STATE_COMPLETE;
    g_test_fatal_hook.result.done = 1U;

    while (1)
    {
    }
}

static void fatal_hook_task(void *argument)
{
    (void)argument;
    kernel_fatal(JRT_FATAL_KERNEL_INVARIANT);
}

JRT_DECLARE_STATIC_TASK_STACK(fatal_hook_stack, JRT_TASK_STACK_WORDS);

static const JRT_TaskDefinition_t fatal_hook_tasks[] = {
    JRT_TASK_DEFINITION(fatal_hook_task, 0U, fatal_hook_stack, 1U, "fatal-hook", 0U)
};

#endif

void test_fatal_hook_start(void)
{
#if defined(JUSTRT_TEST_FATAL_HOOK)
    const JRT_KernelConfig_t config = {
        fatal_hook_tasks,
        sizeof(fatal_hook_tasks) / sizeof(fatal_hook_tasks[0])
    };

    g_test_fatal_hook.result.state = TEST_STATE_RUNNING;
    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_fatal_hook.error_code = 2U;
        g_test_fatal_hook.result.fail = 1U;
        g_test_fatal_hook.result.done = 1U;
        return;
    }
    JRT_KernelStart();
#endif
}
