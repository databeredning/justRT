#include "kernel.h"
#include "heartbeat.h"
#include "../board/board.h"

#define RUN_LED_PERIOD_MS 100U

static void heartbeat_task(void)
{
    while (1)
    {
        board_led_toggle();
        sleep_ticks(ms_to_ticks(RUN_LED_PERIOD_MS));
    }
}

static void activity_task(void)
{
    static uint32_t run_count;

    while (1)
    {
        run_count++;
        if ((run_count & 0xFFU) == 0U)
        {
            sleep_ticks(7U);
        }
    }
}

static const task_entry_t heartbeat_tasks[] = {
    heartbeat_task,
    activity_task
};

void heartbeat_example_start(void)
{
    const task_config_t config = {
        heartbeat_tasks,
        sizeof(heartbeat_tasks) / sizeof(heartbeat_tasks[0])
    };

    board_init();
    kernel_start(&config);
}
