#include "kernel.h"
#include "simple.h"

#define SIMPLE_PERIOD_TICKS 10U

simple_result_t g_simple_result TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_simple_worker_runs TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_simple_observer_runs TASK_UNPRIVILEGED_DATA;

static TASK_UNPRIVILEGED void simple_worker_task(void *argument)
{
    (void)argument;
    g_simple_result.state = SIMPLE_STATE_RUNNING;

    while (1)
    {
        g_simple_worker_runs++;
        sleep_ticks(SIMPLE_PERIOD_TICKS);
    }
}

static TASK_UNPRIVILEGED void simple_observer_task(void *argument)
{
    (void)argument;

    while (1)
    {
        g_simple_observer_runs++;
        yield();
    }
}

static const task_definition_t simple_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { simple_worker_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
        "simple-worker", TASK_FLAG_UNPRIVILEGED },
    { simple_observer_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
        "simple-observer", TASK_FLAG_UNPRIVILEGED }
};

void simple_example_start(void)
{
    const kernel_config_t config = {
        simple_tasks,
        sizeof(simple_tasks) / sizeof(simple_tasks[0])
    };

    g_simple_result.state = SIMPLE_STATE_IDLE;
    g_simple_result.runs = 0U;
    g_simple_result.pass = 0U;
    g_simple_result.fail = 0U;
    g_simple_result.done = 0U;
    g_simple_worker_runs = 0U;
    g_simple_observer_runs = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        g_simple_result.fail = 1U;
        return;
    }
    kernel_start();
}
