#ifndef JUSTRT_TEST_COMMON_H
#define JUSTRT_TEST_COMMON_H

#include <stdint.h>

typedef struct
{
    volatile uint32_t state;
    volatile uint32_t runs;
    volatile uint32_t pass;
    volatile uint32_t fail;
    volatile uint32_t done;
} test_result_t;

enum
{
    TEST_STATE_IDLE = 0U,
    TEST_STATE_RUNNING = 1U,
    TEST_STATE_COMPLETE = 2U
};

#endif
