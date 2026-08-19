#include <stdint.h>

#include "kernel.h"
#include "mutex_contention.h"

#define MUTEX_HOLD_TIME_MS 10U

static mutex_t shared_mutex;
static uint32_t shared_counter;
volatile uint32_t g_mutex_owner_operations;
volatile uint32_t g_mutex_contender_operations;
volatile uint32_t g_mutex_error;

static void owner_task(void *argument)
{
    (void)argument;
    while (1)
    {
        if (mutex_lock(&shared_mutex, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_mutex_error = 1U;
            continue;
        }

        shared_counter++;
        g_mutex_owner_operations++;
        sleep_ticks(ms_to_ticks(MUTEX_HOLD_TIME_MS));

        if (mutex_unlock(&shared_mutex) == 0)
        {
            g_mutex_error = 2U;
        }
        sleep_ticks(1U);
    }
}

static void contender_task(void *argument)
{
    (void)argument;
    while (1)
    {
        if (mutex_lock(&shared_mutex, ms_to_ticks(50U)) == 0)
        {
            g_mutex_error = 3U;
        }
        else
        {
            shared_counter++;
            g_mutex_contender_operations++;
            if (mutex_unlock(&shared_mutex) == 0)
            {
                g_mutex_error = 4U;
            }
        }
        sleep_ticks(1U);
    }
}

static const task_definition_t mutex_tasks[] = {
    { owner_task, 0U, 128U, 1U, "mutex-owner", 0U },
    { contender_task, 0U, 128U, 1U, "mutex-contender", 0U }
};

void mutex_contention_start(void)
{
    const kernel_config_t config = {
        mutex_tasks,
        sizeof(mutex_tasks) / sizeof(mutex_tasks[0])
    };

    mutex_init(&shared_mutex);
    shared_counter = 0U;
    g_mutex_owner_operations = 0U;
    g_mutex_contender_operations = 0U;
    g_mutex_error = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
