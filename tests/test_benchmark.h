#ifndef JUSTRT_TEST_BENCHMARK_H
#define JUSTRT_TEST_BENCHMARK_H

#include "test_common.h"

#include <stdint.h>

typedef struct
{
    test_result_t result;
    volatile uint32_t enumeration_checked;
    volatile uint32_t names_checked;
    volatile uint32_t periods_checked;
    volatile uint32_t reset_checked;
    volatile uint32_t releases_checked;
    volatile uint32_t completions_checked;
    volatile uint32_t timer_service_checked;
    volatile uint32_t error_code;
} benchmark_test_state_t;

extern benchmark_test_state_t g_test_benchmark;

void test_benchmark_start(void);

#endif
