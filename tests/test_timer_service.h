#ifndef JUSTRT_TEST_TIMER_SERVICE_H
#define JUSTRT_TEST_TIMER_SERVICE_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t one_shot_callbacks;
    volatile uint32_t periodic_callbacks;
    volatile uint32_t accumulated_first_callbacks;
    volatile uint32_t accumulated_second_callbacks;
    volatile uint32_t restart_callbacks;
    volatile uint32_t starter_callbacks;
    volatile uint32_t target_callbacks;
    volatile uint32_t polling_expirations;
    volatile uint32_t compatibility_callbacks;
    volatile uint32_t context_checks;
    volatile uint32_t callback_task_index;
    volatile uint32_t error_code;
} timer_service_test_state_t;

void test_timer_service_start(void);

extern timer_service_test_state_t g_test_timer_service;

#endif
