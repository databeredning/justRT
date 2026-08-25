#ifndef JUSTRT_TEST_SYNCHRONIZATION_H
#define JUSTRT_TEST_SYNCHRONIZATION_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t semaphore_received;
    volatile uint32_t queue_received;
    volatile uint32_t event_received;
    volatile uint32_t notification_received;
    volatile uint32_t error_code;
} synchronization_test_state_t;

void test_synchronization_start(void);

extern synchronization_test_state_t g_test_synchronization;

#endif
