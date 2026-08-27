#ifndef JUSTRT_TEST_FPU_H
#define JUSTRT_TEST_FPU_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t task_a_checks;
    volatile uint32_t task_b_checks;
    volatile uint32_t non_fp_runs;
    volatile uint32_t error_code;
} test_fpu_result_t;

void test_fpu_start(void);

extern test_fpu_result_t g_test_fpu;

#endif
