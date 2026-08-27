#include "examples/simple.h"
#include "tests/test_boot_and_privilege.h"
#include "tests/test_fpu.h"
#include "tests/test_mutex.h"
#include "tests/test_synchronization.h"

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
#else
    simple_example_start();
#endif

    return 0;
}
