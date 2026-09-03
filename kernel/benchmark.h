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
void task_benchmark_release_locked(uint32_t task_id, int coalesced);
void task_benchmark_coalesced_locked(uint32_t task_id);
void task_benchmark_start_locked(uint32_t task_id);
void task_benchmark_complete_locked(uint32_t task_id);
JRT_Status_t JRT_BenchmarkGetInfo(JRT_BenchmarkInfo_t *info);
JRT_Status_t JRT_BenchmarkGetTask(uint32_t task_id,
                                  JRT_TaskBenchmarkInfo_t *info);
JRT_Status_t JRT_BenchmarkReset(void);
uint32_t task_benchmark_get_task_flags(uint32_t task_id);
int task_benchmark_is_task_context(void);

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

static inline void task_benchmark_release_locked(uint32_t task_id, int coalesced)
{
    (void)task_id;
    (void)coalesced;
}

static inline void task_benchmark_start_locked(uint32_t task_id)
{
    (void)task_id;
}

static inline void task_benchmark_coalesced_locked(uint32_t task_id)
{
    (void)task_id;
}

static inline void task_benchmark_complete_locked(uint32_t task_id)
{
    (void)task_id;
}

#endif

#endif
