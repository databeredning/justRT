#ifndef JUSTRT_TEST_BOOT_AND_PRIVILEGE_H
#define JUSTRT_TEST_BOOT_AND_PRIVILEGE_H

#include "test_common.h"

void test_boot_and_privilege_start(void);

extern test_result_t g_test_boot_and_privilege;
extern volatile uint32_t g_test_boot_argument;

#endif
