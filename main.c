#include "examples/simple.h"
#include "tests/test_boot_and_privilege.h"
#include "tests/test_fpu.h"
#include "tests/test_mutex.h"
#include "tests/test_race.h"
#include "tests/test_stack_guard.h"
#include "tests/test_synchronization.h"
#include "tests/test_task_capacity.h"
#include "tests/test_timer_service.h"

int main(void)
{
#if defined(JUSTRT_TEST_BOOT)
    test_boot_and_privilege_start();
#elif defined(JUSTRT_TEST_SYNC)
    test_synchronization_start();
#elif defined(JUSTRT_TEST_MUTEX)
    test_mutex_start();
#elif defined(JUSTRT_TEST_FPU)
    test_fpu_start();
#elif defined(JUSTRT_TEST_RACE)
    test_race_start();
#elif defined(JUSTRT_TEST_TIMER_SERVICE)
    test_timer_service_start();
#elif defined(JUSTRT_TEST_TASK_CAPACITY)
    test_task_capacity_start();
#elif defined(JUSTRT_TEST_STACK_GUARD)
    test_stack_guard_start();
#else
    simple_example_start();
#endif

    return 0;
}
