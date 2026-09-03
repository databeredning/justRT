#ifndef JUSTRT_BENCHMARK_H
#define JUSTRT_BENCHMARK_H

#include <stdint.h>

#include "kernel.h"

#if JRT_ENABLE_TASK_BENCHMARK

typedef struct
{
    uint32_t release_count;
    uint32_t completion_count;
    uint32_t coalesced_count;
    uint32_t release_cycle;
    uint32_t activation_start_cycle;
    uint32_t max_release_latency_cycles;
    uint32_t max_activation_cycles;
    uint32_t period_cycles;
    uint8_t release_pending;
    uint8_t activation_active;
} JRT_TaskBenchmarkRecord_t;

void task_benchmark_init(uint32_t task_count,
                         const JRT_TaskDefinition_t *definitions,
                         uint32_t application_task_count);

#else

static inline void task_benchmark_init(
    uint32_t task_count,
    const JRT_TaskDefinition_t *definitions,
    uint32_t application_task_count)
{
    (void)task_count;
    (void)definitions;
    (void)application_task_count;
}

#endif

#endif
