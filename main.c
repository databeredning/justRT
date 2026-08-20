#include <stdint.h>
#include "examples/heartbeat.h"
#include "examples/isr_sync_paths.h"

#define MAIN_PROFILE_BRINGUP 0U
#define MAIN_PROFILE_REGRESSION_ISR_SYNC 1U

#ifndef JUSTBOOT_MAIN_PROFILE
#define JUSTBOOT_MAIN_PROFILE MAIN_PROFILE_BRINGUP
#endif

int main(void)
{
#if JUSTBOOT_MAIN_PROFILE == MAIN_PROFILE_REGRESSION_ISR_SYNC
    isr_sync_paths_start();
#else
    heartbeat_example_start();
#endif

    return 0;
}
