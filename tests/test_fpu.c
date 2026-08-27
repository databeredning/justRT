#include "kernel.h"
#include "test_fpu.h"

#define TEST_FPU_PATTERN_A 0x3F100100U
#define TEST_FPU_PATTERN_B 0x40A00200U
#define TEST_FPU_CHECK_TARGET 1000U
#define TEST_FPU_BUSY_ITERATIONS 128U

enum
{
    TEST_FPU_ERROR_TASK_A = 1U,
    TEST_FPU_ERROR_TASK_B = 2U
};

void test_fpu_load_registers(uint32_t pattern);
uint32_t test_fpu_check_registers(uint32_t pattern);

test_fpu_result_t g_test_fpu JRT_TASK_UNPRIVILEGED_DATA;

static JRT_TASK_UNPRIVILEGED void test_fpu_busy_work(void)
{
    volatile uint32_t value = 0U;
    uint32_t index;

    for (index = 0U; index < TEST_FPU_BUSY_ITERATIONS; index++)
    {
        value += index;
    }
    (void)value;
}

static JRT_TASK_UNPRIVILEGED void test_fpu_task_a(void *argument)
{
    (void)argument;
    test_fpu_load_registers(TEST_FPU_PATTERN_A);

    while (1)
    {
        test_fpu_busy_work();
        JRT_TaskYield();
        if (test_fpu_check_registers(TEST_FPU_PATTERN_A) == 0U)
        {
            g_test_fpu.result.fail = 1U;
            g_test_fpu.error_code = TEST_FPU_ERROR_TASK_A;
        }
        g_test_fpu.task_a_checks++;
    }
}

static JRT_TASK_UNPRIVILEGED void test_fpu_task_b(void *argument)
{
    (void)argument;
    test_fpu_load_registers(TEST_FPU_PATTERN_B);

    while (1)
    {
        test_fpu_busy_work();
        JRT_TaskYield();
        if (test_fpu_check_registers(TEST_FPU_PATTERN_B) == 0U)
        {
            g_test_fpu.result.fail = 1U;
            g_test_fpu.error_code = TEST_FPU_ERROR_TASK_B;
        }
        g_test_fpu.task_b_checks++;
    }
}

static JRT_TASK_UNPRIVILEGED void test_fpu_non_fp_task(void *argument)
{
    (void)argument;
    g_test_fpu.result.state = TEST_STATE_RUNNING;

    while ((g_test_fpu.task_a_checks < TEST_FPU_CHECK_TARGET)
           || (g_test_fpu.task_b_checks < TEST_FPU_CHECK_TARGET))
    {
        g_test_fpu.non_fp_runs++;
        JRT_TaskYield();
    }

    g_test_fpu.result.runs = g_test_fpu.task_a_checks
                           + g_test_fpu.task_b_checks;
    if ((g_test_fpu.result.fail == 0U) && (g_test_fpu.non_fp_runs != 0U))
    {
        g_test_fpu.result.pass = 1U;
    }
    else
    {
        g_test_fpu.result.fail = 1U;
    }
    g_test_fpu.result.done = 1U;
    g_test_fpu.result.state = TEST_STATE_COMPLETE;

    while (1)
    {
        JRT_TaskYield();
    }
}

static const JRT_TaskDefinition_t test_fpu_tasks[] JRT_TASK_UNPRIVILEGED_RODATA = {
    { test_fpu_task_a, 0U, JRT_TASK_STACK_WORDS, 1U,
        "test-fpu-a", JRT_TASK_FLAG_UNPRIVILEGED },
    { test_fpu_task_b, 0U, JRT_TASK_STACK_WORDS, 1U,
        "test-fpu-b", JRT_TASK_FLAG_UNPRIVILEGED },
    { test_fpu_non_fp_task, 0U, JRT_TASK_STACK_WORDS, 1U,
        "test-non-fp", JRT_TASK_FLAG_UNPRIVILEGED }
};

void test_fpu_start(void)
{
    const JRT_KernelConfig_t config = {
        test_fpu_tasks,
        sizeof(test_fpu_tasks) / sizeof(test_fpu_tasks[0])
    };

    g_test_fpu.result.state = TEST_STATE_IDLE;
    g_test_fpu.result.runs = 0U;
    g_test_fpu.result.pass = 0U;
    g_test_fpu.result.fail = 0U;
    g_test_fpu.result.done = 0U;
    g_test_fpu.task_a_checks = 0U;
    g_test_fpu.task_b_checks = 0U;
    g_test_fpu.non_fp_runs = 0U;
    g_test_fpu.error_code = 0U;

    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_fpu.result.fail = 1U;
        return;
    }
    JRT_KernelStart();
}
