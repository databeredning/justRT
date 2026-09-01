#ifndef JUSTRT_TEST_PRIVATE_CONFIG_H
#define JUSTRT_TEST_PRIVATE_CONFIG_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t invalid_cases_rejected;
    volatile uint32_t valid_config_accepted;
    volatile uint32_t tasks_ran;
    volatile uint32_t task_a_value;
    volatile uint32_t task_b_value;
    volatile uint32_t error_code;
} private_config_test_state_t;

void test_private_config_start(void);

extern private_config_test_state_t g_test_private_config;

#endif
