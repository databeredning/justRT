#ifndef JUSTRT_TEST_MUTEX_H
#define JUSTRT_TEST_MUTEX_H

#include <stdint.h>

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t recursive_lock_first;
    volatile uint32_t recursive_lock_second;
    volatile uint32_t recursive_unlock_first;
    volatile uint32_t recursive_unlock_second;
    volatile uint32_t non_owner_unlock;
    volatile uint32_t owner_priority_full_chain;
    volatile uint32_t bridge_blocked;
    volatile uint32_t high_blocked;
    volatile uint32_t bridge_acquired_first;
    volatile uint32_t high_acquired_second;
    volatile uint32_t error_code;
} mutex_test_state_t;

void test_mutex_start(void);

extern mutex_test_state_t g_test_mutex;

#endif
