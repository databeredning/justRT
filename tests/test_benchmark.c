#include "kernel.h"
#include "test_benchmark.h"

benchmark_test_state_t g_test_benchmark JRT_TASK_UNPRIVILEGED_DATA;

#if JRT_ENABLE_TASK_BENCHMARK

static void benchmark_worker(void *argument)
{
    uint32_t period_ticks = (uint32_t)(uintptr_t)argument;

    while (1)
    {
        JRT_TaskDelay(period_ticks);
    }
}

static void benchmark_notification_receiver(void *argument)
{
    uint32_t value;

    (void)argument;
    while (1)
    {
        JRT_TaskNotifyTake(&value, JRT_WAIT_FOREVER);
    }
}

static void benchmark_notification_sender(void *argument)
{
    (void)argument;
    JRT_TaskDelay(25U);
    while (1)
    {
        JRT_TaskNotify(4U, 1U);
        JRT_TaskNotify(4U, 1U);
        JRT_TaskDelay(5U);
    }
}

static void benchmark_controller(void *argument)
{
    JRT_BenchmarkInfo_t benchmark_info;
    JRT_TaskBenchmarkInfo_t task_info;
    JRT_TaskStackInfo_t stack_info;
    const char *name;
    uint32_t index;
    uint32_t valid = 1U;

    (void)argument;
    JRT_TaskDelay(20U);
    if (JRT_BenchmarkReset() != JRT_STATUS_OK)
    {
        valid = 0U;
    }
    else
    {
        g_test_benchmark.reset_checked = 1U;
    }
    if ((JRT_BenchmarkGetTask(0U, &task_info) != JRT_STATUS_OK)
        || (task_info.release_count != 0U)
        || (task_info.completion_count != 0U)
        || (task_info.max_activation_cycles != 0U))
    {
        valid = 0U;
    }
    JRT_TaskDelay(20U);
    if ((JRT_BenchmarkGetInfo(&benchmark_info) != JRT_STATUS_OK)
        || (benchmark_info.enabled == 0U)
        || (benchmark_info.task_count != 7U)
        || (benchmark_info.cycle_frequency_hz == 0U))
    {
        valid = 0U;
    }
    else
    {
        g_test_benchmark.enumeration_checked = 1U;
    }
    for (index = 0U; index < 6U; index++)
    {
        if ((JRT_TaskGetName(index, &name) != JRT_STATUS_OK)
            || (name == 0U)
            || (JRT_BenchmarkGetTask(index, &task_info) != JRT_STATUS_OK)
            || (task_info.flags != 0U)
            || (task_info.stack_words == 0U)
            || (task_info.used_stack_words > task_info.stack_words)
            || (JRT_TaskGetStackInfo(index, &stack_info) != JRT_STATUS_OK)
            || (task_info.stack_words != stack_info.stack_words)
            || (task_info.used_stack_words != stack_info.used_words)
            || ((index < 3U) && (task_info.period_cycles == 0U))
            || ((index >= 3U) && (task_info.period_cycles != 0U)))
        {
            valid = 0U;
        }
    }
    if (valid != 0U)
    {
        g_test_benchmark.names_checked = 1U;
        g_test_benchmark.periods_checked = 1U;
    }
    if ((JRT_BenchmarkGetTask(6U, &task_info) != JRT_STATUS_OK)
        || (task_info.period_cycles != 0U))
    {
        valid = 0U;
    }
    else
    {
        g_test_benchmark.timer_service_checked = 1U;
    }
    if ((JRT_BenchmarkGetTask(0U, &task_info) == JRT_STATUS_OK)
        && (task_info.release_count != 0U)
        && (task_info.completion_count != 0U))
    {
        g_test_benchmark.releases_checked = 1U;
        g_test_benchmark.completions_checked = 1U;
    }
    else
    {
        valid = 0U;
    }
    if ((JRT_BenchmarkGetTask(4U, &task_info) == JRT_STATUS_OK)
        && (task_info.coalesced_count != 0U))
    {
        g_test_benchmark.coalescing_checked = 1U;
    }
    else
    {
        valid = 0U;
    }
    if ((JRT_BenchmarkGetInfo(0U) != JRT_STATUS_INVALID_TASK)
        || (JRT_BenchmarkGetTask(7U, &task_info) != JRT_STATUS_INVALID_TASK))
    {
        valid = 0U;
    }
    g_test_benchmark.result.runs = 1U;
    g_test_benchmark.result.pass = valid;
    g_test_benchmark.result.fail = (valid == 0U) ? 1U : 0U;
    g_test_benchmark.error_code = (valid == 0U) ? 1U : 0U;
    g_test_benchmark.result.state = TEST_STATE_COMPLETE;
    g_test_benchmark.result.done = 1U;
    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

JRT_DECLARE_STATIC_TASK_STACK(benchmark_worker_a_stack, 128U);
JRT_DECLARE_STATIC_TASK_STACK(benchmark_worker_b_stack, 128U);
JRT_DECLARE_STATIC_TASK_STACK(benchmark_worker_c_stack, 128U);
JRT_DECLARE_STATIC_TASK_STACK(benchmark_controller_stack, 128U);
JRT_DECLARE_STATIC_TASK_STACK(benchmark_receiver_stack, 128U);
JRT_DECLARE_STATIC_TASK_STACK(benchmark_sender_stack, 128U);

static const JRT_TaskDefinition_t benchmark_tasks[] = {
    JRT_TASK_DEFINITION_WITH_PERIOD(
        benchmark_worker, (void *)2U, benchmark_worker_a_stack,
        1U, "benchmark-a", 0U, 2U),
    JRT_TASK_DEFINITION_WITH_PERIOD(
        benchmark_worker, (void *)3U, benchmark_worker_b_stack,
        2U, "benchmark-b", 0U, 3U),
    JRT_TASK_DEFINITION_WITH_PERIOD(
        benchmark_worker, (void *)5U, benchmark_worker_c_stack,
        1U, "benchmark-c", 0U, 5U),
    JRT_TASK_DEFINITION_WITH_PERIOD(
        benchmark_controller, 0U, benchmark_controller_stack,
        3U, "benchmark-controller", 0U, 0U),
    JRT_TASK_DEFINITION_WITH_PERIOD(
        benchmark_notification_receiver, 0U, benchmark_receiver_stack,
        1U, "benchmark-receiver", 0U, 0U),
    JRT_TASK_DEFINITION_WITH_PERIOD(
        benchmark_notification_sender, 0U, benchmark_sender_stack,
        4U, "benchmark-sender", 0U, 0U)
};

void test_benchmark_start(void)
{
    const JRT_KernelConfig_t config = {
        benchmark_tasks,
        sizeof(benchmark_tasks) / sizeof(benchmark_tasks[0])
    };

    g_test_benchmark.result.state = TEST_STATE_RUNNING;
    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_benchmark.result.fail = 1U;
        g_test_benchmark.result.done = 1U;
        return;
    }
    JRT_KernelStart();
}

#else

void test_benchmark_start(void)
{
    g_test_benchmark.result.fail = 1U;
    g_test_benchmark.result.done = 1U;
}

#endif
