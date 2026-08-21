#include "kernel.h"
#include "heartbeat.h"
#include "../board/board.h"

#define RUN_LED_PERIOD_MS 100U

volatile uint32_t g_periodic_delay_runs;
volatile uint32_t g_periodic_delay_last_tick;

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

static TASK_UNPRIVILEGED void unprivileged_led_task(void *argument)
{
    (void)argument;
    while (1)
    {
        led_toggle();
        sleep_ticks(ms_to_ticks(RUN_LED_PERIOD_MS));
    }
}

static void periodic_delay_task(void *argument)
{
    uint32_t previous_wake = kernel_ticks_now();

    (void)argument;
    while (1)
    {
        task_delay_until(&previous_wake, ms_to_ticks(RUN_LED_PERIOD_MS));
        g_periodic_delay_runs++;
        g_periodic_delay_last_tick = kernel_ticks_now();
    }
}

static const task_definition_t heartbeat_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { heartbeat_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "heartbeat", 0U },
    { activity_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "activity", 0U }
};

static const task_definition_t unprivileged_led_tasks[] TASK_UNPRIVILEGED_RODATA = {
        { unprivileged_led_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
            "unprivileged-led", TASK_FLAG_UNPRIVILEGED }
};

static const task_definition_t periodic_delay_tasks[] TASK_UNPRIVILEGED_RODATA = {
        { periodic_delay_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
            "periodic-delay", 0U }
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

void heartbeat_unprivileged_led_start(void)
{
    const kernel_config_t config = {
        unprivileged_led_tasks,
        sizeof(unprivileged_led_tasks) / sizeof(unprivileged_led_tasks[0])
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

void heartbeat_periodic_delay_start(void)
{
    const kernel_config_t config = {
        periodic_delay_tasks,
        sizeof(periodic_delay_tasks) / sizeof(periodic_delay_tasks[0])
    };

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
