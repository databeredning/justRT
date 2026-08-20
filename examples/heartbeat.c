#include "kernel.h"
#include "heartbeat.h"
#include "../board/board.h"

#define RUN_LED_PERIOD_MS 100U

static TASK_UNPRIVILEGED void heartbeat_task(void *argument)
{
    (void)argument;
    while (1)
    {
        board_led_toggle();
        sleep_ticks(ms_to_ticks(RUN_LED_PERIOD_MS));
    }
}

static TASK_UNPRIVILEGED void activity_task(void *argument)
{
    static uint32_t run_count;

    (void)argument;

    while (1)
    {
        run_count++;
        if ((run_count & 0xFFU) == 0U)
        {
            sleep_ticks(7U);
        }
    }
}

static const task_definition_t heartbeat_tasks[] = {
    { heartbeat_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "heartbeat", 0U },
    { activity_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "activity", 0U }
};

void heartbeat_example_start(void)
{
    const kernel_config_t config = {
        heartbeat_tasks,
        sizeof(heartbeat_tasks) / sizeof(heartbeat_tasks[0])
    };

    board_init();
    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
