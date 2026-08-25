#include "kernel.h"
#include "simple.h"

#define SIMPLE_PERIOD_TICKS 10U

simple_result_t g_simple_result JRT_TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_simple_worker_runs JRT_TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_simple_observer_runs JRT_TASK_UNPRIVILEGED_DATA;

static JRT_TASK_UNPRIVILEGED void simple_worker_task(void *argument)
{
    (void)argument;
    g_simple_result.state = SIMPLE_STATE_RUNNING;

    while (1)
    {
        g_simple_worker_runs++;
        JRT_TaskDelay(SIMPLE_PERIOD_TICKS);
    }
}

static JRT_TASK_UNPRIVILEGED void simple_observer_task(void *argument)
{
    (void)argument;

    while (1)
    {
        g_simple_observer_runs++;
        JRT_TaskYield();
    }
}

static const JRT_TaskDefinition_t simple_tasks[] JRT_TASK_UNPRIVILEGED_RODATA = {
    { simple_worker_task, 0U, JRT_TASK_STACK_WORDS, 1U,
        "simple-worker", JRT_TASK_FLAG_UNPRIVILEGED },
    { simple_observer_task, 0U, JRT_TASK_STACK_WORDS, 1U,
        "simple-observer", JRT_TASK_FLAG_UNPRIVILEGED }
};

void simple_example_start(void)
{
    const JRT_KernelConfig_t config = {
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

    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_simple_result.fail = 1U;
        return;
    }
    JRT_KernelStart();
}
