#ifndef JUSTRT_TEST_TASK_CAPACITY_H
#define JUSTRT_TEST_TASK_CAPACITY_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t configured_limit;
    volatile uint32_t tasks_ran;
    volatile uint32_t maximum_accepted;
    volatile uint32_t maximum_plus_one_rejected;
    volatile uint32_t guard_updates;
    volatile uint32_t guard_base_matches;
    volatile uint32_t ready_scan_depth;
    volatile uint32_t scheduler_pass2_max;
    volatile uint32_t error_code;
} task_capacity_test_state_t;

void test_task_capacity_start(void);

extern task_capacity_test_state_t g_test_task_capacity;

#endif
