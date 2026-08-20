#include <stdint.h>

#include "kernel.h"
#include "semaphore_event.h"

#define SEMAPHORE_EVENT_PERIOD_MS 100U

static semaphore_t event_semaphore;
volatile uint32_t g_semaphore_events_sent;
volatile uint32_t g_semaphore_events_received;
volatile uint32_t g_semaphore_event_error;

static TASK_UNPRIVILEGED void event_source_task(void *argument)
{
    (void)argument;
    while (1)
    {
        semaphore_give(&event_semaphore);
        g_semaphore_events_sent++;
        sleep_ticks(ms_to_ticks(SEMAPHORE_EVENT_PERIOD_MS));
    }
}

static TASK_UNPRIVILEGED void event_worker_task(void *argument)
{
    (void)argument;
    while (1)
    {
        if (semaphore_take(&event_semaphore, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_semaphore_event_error = 1U;
        }
        else
        {
            g_semaphore_events_received++;
        }
    }
}

static const task_definition_t semaphore_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { event_source_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "event-source", 0U },
    { event_worker_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "event-worker", 0U }
};

void semaphore_event_start(void)
{
    const kernel_config_t config = {
        semaphore_tasks,
        sizeof(semaphore_tasks) / sizeof(semaphore_tasks[0])
    };

    semaphore_init(&event_semaphore, 0U);
    g_semaphore_events_sent = 0U;
    g_semaphore_events_received = 0U;
    g_semaphore_event_error = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
