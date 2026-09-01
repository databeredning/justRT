#ifndef JUSTRT_TEST_TASK_SUSPENSION_H
#define JUSTRT_TEST_TASK_SUSPENSION_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t isr_status;
    volatile uint32_t self_suspend_entered;
    volatile uint32_t self_suspend_returned;
    volatile uint32_t suspend_again_rejected;
    volatile uint32_t resume_accepted;
    volatile uint32_t resume_again_rejected;
    volatile uint32_t suspend_other_accepted;
    volatile uint32_t suspended_progress_stable;
    volatile uint32_t sleeping_rejected;
    volatile uint32_t blocked_rejected;
    volatile uint32_t invalid_id_rejected;
    volatile uint32_t internal_id_rejected;
    volatile uint32_t resume_self_rejected;
    volatile uint32_t stack_state_preserved;
    volatile uint32_t private_state_restored;
    volatile uint32_t shared_accesses;
    volatile uint32_t worker_complete;
    volatile uint32_t expected_fault_address;
    volatile uint32_t cross_read_attempted;
    volatile uint32_t error_code;
} task_suspension_test_state_t;

void test_task_suspension_start(void);

extern task_suspension_test_state_t g_test_task_suspension;

#endif
