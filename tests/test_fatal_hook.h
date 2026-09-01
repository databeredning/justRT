#ifndef JUSTRT_TEST_FATAL_HOOK_H
#define JUSTRT_TEST_FATAL_HOOK_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t hook_calls;
    volatile uint32_t hook_reason;
    volatile uint32_t interrupts_masked;
    volatile uint32_t error_code;
} fatal_hook_test_state_t;

void test_fatal_hook_start(void);

extern fatal_hook_test_state_t g_test_fatal_hook;

#endif
