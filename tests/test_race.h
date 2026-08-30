#ifndef JUSTRT_TEST_RACE_H
#define JUSTRT_TEST_RACE_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t signals;
    volatile uint32_t takes;
    volatile uint32_t timeouts;
    volatile uint32_t lost_wakeups;
    volatile uint32_t error_code;
} race_test_state_t;

void test_race_start(void);

extern race_test_state_t g_test_race;

#endif
