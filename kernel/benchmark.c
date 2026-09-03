#include "benchmark.h"
#include "cortex_m/port_contract.h"

#if JRT_ENABLE_TASK_BENCHMARK

#define JRT_MAX_BENCHMARK_TASKS (JRT_MAX_APPLICATION_TASKS + 3U)

static JRT_TaskBenchmarkRecord_t
    benchmark_records[JRT_MAX_BENCHMARK_TASKS] KERNEL_PRIVILEGED_DATA;
static uint32_t benchmark_cycle_counter_available KERNEL_PRIVILEGED_DATA;
static uint32_t benchmark_task_count KERNEL_PRIVILEGED_DATA;
static uint32_t benchmark_cycle_frequency_hz KERNEL_PRIVILEGED_DATA;

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
    benchmark_task_count = application_task_count + 1U;
    benchmark_cycle_frequency_hz = benchmark_cycle_counter_available
                                       ? JRT_CORE_CLOCK_HZ : 0U;
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

JRT_Status_t JRT_BenchmarkGetInfo(JRT_BenchmarkInfo_t *info)
{
    uint32_t saved_primask;

    if (info == 0U)
    {
        return JRT_STATUS_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    info->enabled = benchmark_cycle_counter_available;
    info->task_count = benchmark_task_count;
    info->cycle_frequency_hz = benchmark_cycle_frequency_hz;
    arch_critical_exit(saved_primask);
    return JRT_STATUS_OK;
}

JRT_Status_t JRT_BenchmarkGetTask(uint32_t task_id,
                                  JRT_TaskBenchmarkInfo_t *info)
{
    JRT_TaskBenchmarkRecord_t record;
    JRT_TaskStackInfo_t stack_info;
    uint32_t saved_primask;
    uint32_t completed_count;

    if (info == 0U)
    {
        return JRT_STATUS_INVALID_TASK;
    }
    saved_primask = arch_critical_enter();
    if (task_id >= benchmark_task_count)
    {
        arch_critical_exit(saved_primask);
        return JRT_STATUS_INVALID_TASK;
    }
    record = benchmark_records[task_id];
    if ((record.completion_count > record.release_count)
        || (record.coalesced_count
            > (record.release_count - record.completion_count)))
    {
        info->pending_count = 0U;
    }
    else
    {
        completed_count = record.completion_count + record.coalesced_count;
        info->pending_count = record.release_count - completed_count;
    }
    info->release_count = record.release_count;
    info->completion_count = record.completion_count;
    info->coalesced_count = record.coalesced_count;
    info->max_release_latency_cycles = record.max_release_latency_cycles;
    info->max_activation_cycles = record.max_activation_cycles;
    info->period_cycles = record.period_cycles;
    info->flags = task_benchmark_get_task_flags(task_id);
    if (JRT_TaskGetStackInfo(task_id, &stack_info) != JRT_STATUS_OK)
    {
        arch_critical_exit(saved_primask);
        return JRT_STATUS_NOT_INITIALIZED;
    }
    info->stack_words = stack_info.stack_words;
    info->used_stack_words = stack_info.used_words;
    arch_critical_exit(saved_primask);
    return JRT_STATUS_OK;
}

JRT_Status_t JRT_BenchmarkReset(void)
{
    uint32_t saved_primask;
    uint32_t index;
    uint32_t now;

    if (task_benchmark_is_task_context() == 0)
    {
        return JRT_STATUS_INVALID_CONTEXT;
    }
    saved_primask = arch_critical_enter();
    now = arch_cycle_counter_read();
    for (index = 0U; index < benchmark_task_count; index++)
    {
        JRT_TaskBenchmarkRecord_t *record = &benchmark_records[index];

        record->release_count = 0U;
        record->completion_count = 0U;
        record->coalesced_count = 0U;
        record->max_release_latency_cycles = 0U;
        record->max_activation_cycles = 0U;
        if (record->release_pending != 0U)
        {
            record->release_cycle = now;
        }
        if (record->activation_active != 0U)
        {
            record->activation_start_cycle = now;
        }
    }
    arch_critical_exit(saved_primask);
    return JRT_STATUS_OK;
}

#endif
