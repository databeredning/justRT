#include "benchmark.h"

#if JRT_ENABLE_TASK_BENCHMARK

#define JRT_INTERNAL_TASK_COUNT 2U
#define JRT_MAX_BENCHMARK_TASKS (JRT_MAX_APPLICATION_TASKS + 3U)

static JRT_TaskBenchmarkRecord_t
    benchmark_records[JRT_MAX_BENCHMARK_TASKS] KERNEL_PRIVILEGED_DATA;

static uint32_t period_to_cycles(uint32_t period_ticks)
{
    uint64_t period_cycles = (uint64_t)period_ticks
                             * (uint64_t)JRT_CORE_CLOCK_HZ;

    period_cycles /= (uint64_t)JRT_TICK_RATE_HZ;
    return (period_cycles <= UINT32_MAX) ? (uint32_t)period_cycles : 0U;
}

void task_benchmark_init(uint32_t task_count,
                         const JRT_TaskDefinition_t *definitions,
                         uint32_t application_task_count)
{
    uint32_t index;

    for (index = 0U; index < task_count; index++)
    {
        benchmark_records[index] = (JRT_TaskBenchmarkRecord_t){ 0U };
    }
    for (index = 0U; index < application_task_count; index++)
    {
        benchmark_records[index].period_cycles =
            period_to_cycles(definitions[index].benchmark_period_ticks);
    }
}

#endif
