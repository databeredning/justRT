#ifndef JUSTRT_TEST_STACK_GUARD_H
#define JUSTRT_TEST_STACK_GUARD_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t expected_guard_address;
    volatile uint32_t write_attempted;
} stack_guard_test_state_t;

void test_stack_guard_start(void);

extern stack_guard_test_state_t g_test_stack_guard;

#endif
