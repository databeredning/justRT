#ifndef JUSTRT_TEST_CONFIG_RUNTIME_H
#define JUSTRT_TEST_CONFIG_RUNTIME_H

#include "test_common.h"

typedef struct
{
    test_result_t result;
    volatile uint32_t priority_rejected;
    volatile uint32_t conversion_checks;
    volatile uint32_t saturation_checked;
    volatile uint32_t error_code;
} config_runtime_test_state_t;

void test_config_runtime_start(void);

extern config_runtime_test_state_t g_test_config_runtime;

#endif
