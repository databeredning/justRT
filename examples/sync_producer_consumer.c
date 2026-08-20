#include <stdint.h>

#include "kernel.h"
#include "sync_producer_consumer.h"

#define SYNC_QUEUE_CAPACITY 4U

static semaphore_t items_available;
static queue_t values;
static uint32_t value_storage[SYNC_QUEUE_CAPACITY];
volatile uint32_t g_sync_producer_value;
volatile uint32_t g_sync_consumer_value;
volatile uint32_t g_sync_error;

static TASK_UNPRIVILEGED void producer_task(void *argument)
{
    uint32_t value = 0U;

    (void)argument;
    while (1)
    {
        value++;
        if (queue_send(&values, &value, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_sync_error = 1U;
        }
        else
        {
            g_sync_producer_value = value;
        }
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void consumer_task(void *argument)
{
    uint32_t expected = 1U;
    uint32_t value;

    (void)argument;
    while (1)
    {
        if(queue_receive(&values, &value, 0U) != 0)
        {
            if (value != expected)
            {
                g_sync_error = value;
                expected = value + 1U;
            }
            else
            {
                expected++;
            }
            g_sync_consumer_value = value;
        }
        else
        {
            g_sync_error = 2U;
        }
    }
}

static const task_definition_t sync_tasks[] = {
    { producer_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "producer", 0U },
    { consumer_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "consumer", 0U }
};

void sync_producer_consumer_start(void)
{
    const kernel_config_t config = {
        sync_tasks,
        sizeof(sync_tasks) / sizeof(sync_tasks[0])
    };

    semaphore_init(&items_available, 0U);
    queue_init(&values, value_storage, SYNC_QUEUE_CAPACITY, sizeof(uint32_t));
    g_sync_producer_value = 0U;
    g_sync_consumer_value = 0U;
    g_sync_error = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
