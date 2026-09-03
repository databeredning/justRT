#include "benchmark.h"
#include "cortex_m/port_contract.h"

#if JRT_ENABLE_TASK_BENCHMARK

#define JRT_MAX_BENCHMARK_TASKS (JRT_MAX_APPLICATION_TASKS + 3U)

static JRT_TaskBenchmarkRecord_t
    benchmark_records[JRT_MAX_BENCHMARK_TASKS] KERNEL_PRIVILEGED_DATA;
static uint32_t benchmark_cycle_counter_available KERNEL_PRIVILEGED_DATA;

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

    arch_cycle_counter_init();
    benchmark_cycle_counter_available =
        (uint32_t)arch_cycle_counter_available();
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

void task_benchmark_release_locked(uint32_t task_id, int coalesced)
{
    JRT_TaskBenchmarkRecord_t *record;

    if (benchmark_cycle_counter_available == 0U)
    {
        return;
    }
    record = &benchmark_records[task_id];
    record->release_count++;
    if (coalesced != 0)
    {
        record->coalesced_count++;
    }
    if (record->release_pending == 0U)
    {
        record->release_cycle = arch_cycle_counter_read();
        record->release_pending = 1U;
    }
}

void task_benchmark_start_locked(uint32_t task_id)
{
    JRT_TaskBenchmarkRecord_t *record;
    uint32_t now;
    uint32_t latency;

    if (benchmark_cycle_counter_available == 0U)
    {
        return;
    }
    record = &benchmark_records[task_id];
    if ((record->release_pending == 0U)
        || (record->activation_active != 0U))
    {
        return;
    }
    now = arch_cycle_counter_read();
    latency = now - record->release_cycle;
    if (latency > record->max_release_latency_cycles)
    {
        record->max_release_latency_cycles = latency;
    }
    record->activation_start_cycle = now;
    record->release_pending = 0U;
    record->activation_active = 1U;
}

void task_benchmark_complete_locked(uint32_t task_id)
{
    JRT_TaskBenchmarkRecord_t *record;
    uint32_t duration;

    if (benchmark_cycle_counter_available == 0U)
    {
        return;
    }
    record = &benchmark_records[task_id];
    if (record->activation_active == 0U)
    {
        return;
    }
    duration = arch_cycle_counter_read() - record->activation_start_cycle;
    if (duration > record->max_activation_cycles)
    {
        record->max_activation_cycles = duration;
    }
    record->completion_count++;
    record->activation_active = 0U;
}

#endif
