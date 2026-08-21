#include "kernel.h"
#include "heartbeat.h"
#include "../board/board.h"

#define RUN_LED_PERIOD_MS 100U

volatile uint32_t g_privilege_probe_runs TASK_UNPRIVILEGED_DATA;

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

static TASK_UNPRIVILEGED void privilege_counter_task(void *argument)
{
    (void)argument;
    while (1)
    {
        g_privilege_probe_runs++;
    }
}

static TASK_UNPRIVILEGED void unprivileged_svc_task(void *argument)
{
    (void)argument;
    while (1)
    {
        g_privilege_probe_runs++;
        sleep_ticks(1U);
    }
}

static const task_definition_t heartbeat_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { heartbeat_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "heartbeat", 0U },
    { activity_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "activity", 0U }
};

static const task_definition_t privilege_counter_tasks[] TASK_UNPRIVILEGED_RODATA = {
        { privilege_counter_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
            "privilege-counter", TASK_FLAG_UNPRIVILEGED }
};

static const task_definition_t unprivileged_svc_tasks[] TASK_UNPRIVILEGED_RODATA = {
        { unprivileged_svc_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
            "unprivileged-svc", TASK_FLAG_UNPRIVILEGED }
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

void heartbeat_privilege_counter_start(void)
{
    const kernel_config_t config = {
        privilege_counter_tasks,
        sizeof(privilege_counter_tasks) / sizeof(privilege_counter_tasks[0])
    };

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void heartbeat_unprivileged_svc_start(void)
{
    const kernel_config_t config = {
        unprivileged_svc_tasks,
        sizeof(unprivileged_svc_tasks) / sizeof(unprivileged_svc_tasks[0])
    };

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
