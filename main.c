#include <stdint.h>
#include "examples/heartbeat.h"
#include "examples/isr_sync_paths.h"

#define MAIN_PROFILE_BRINGUP 0U
#define MAIN_PROFILE_REGRESSION_ISR_SYNC 1U
#define MAIN_PROFILE_REGRESSION_ISR_QFULL 2U
#define MAIN_PROFILE_SOAK 3U
#define MAIN_PROFILE_UNPRIVILEGED_LED 6U
#define MAIN_PROFILE_PERIODIC_DELAY 7U
#define MAIN_PROFILE_TIMER 8U
#define MAIN_PROFILE_TIMER_CALLBACK 9U
#define MAIN_PROFILE_NOTIFICATION 10U

#ifndef JUSTBOOT_MAIN_PROFILE
#define JUSTBOOT_MAIN_PROFILE MAIN_PROFILE_BRINGUP
#endif

int main(void)
{
#if JUSTBOOT_MAIN_PROFILE == MAIN_PROFILE_REGRESSION_ISR_SYNC
    isr_sync_paths_start();
#elif JUSTBOOT_MAIN_PROFILE == MAIN_PROFILE_REGRESSION_ISR_QFULL
    isr_sync_queue_full_start();
#elif JUSTBOOT_MAIN_PROFILE == MAIN_PROFILE_SOAK
    isr_sync_soak_start();
#elif JUSTBOOT_MAIN_PROFILE == MAIN_PROFILE_UNPRIVILEGED_LED
    heartbeat_unprivileged_led_start();
#elif JUSTBOOT_MAIN_PROFILE == MAIN_PROFILE_PERIODIC_DELAY
    heartbeat_periodic_delay_start();
#elif JUSTBOOT_MAIN_PROFILE == MAIN_PROFILE_TIMER
    heartbeat_timer_start();
#elif JUSTBOOT_MAIN_PROFILE == MAIN_PROFILE_TIMER_CALLBACK
    heartbeat_timer_callback_start();
#elif JUSTBOOT_MAIN_PROFILE == MAIN_PROFILE_NOTIFICATION
    heartbeat_notification_start();
#else
    heartbeat_example_start();
#endif

    return 0;
}
