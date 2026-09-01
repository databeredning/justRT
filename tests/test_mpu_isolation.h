#ifndef JUSTRT_TEST_MPU_ISOLATION_H
#define JUSTRT_TEST_MPU_ISOLATION_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t expected_fault_address;
    volatile uint32_t task_a_private_access;
    volatile uint32_t task_b_private_access;
    volatile uint32_t shared_accesses;
    volatile uint32_t context_switch_revoked_access;
    volatile uint32_t cross_read_attempted;
    volatile uint32_t cross_write_attempted;
    volatile uint32_t error_code;
} mpu_isolation_test_state_t;

void test_mpu_isolation_start(void);

extern mpu_isolation_test_state_t g_test_mpu_isolation;

#endif
