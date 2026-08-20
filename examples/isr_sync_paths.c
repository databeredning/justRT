#include <stdint.h>

#include "kernel.h"
#include "isr_sync_paths.h"

#define ISR_QUEUE_CAPACITY 8U
#define ISR_HOOK_PERIOD_TICKS 4U
#define ISR_EXPECTED_EVENTS 32U

static semaphore_t isr_semaphore;
static queue_t isr_queue;
static uint32_t isr_queue_storage[ISR_QUEUE_CAPACITY];

volatile uint32_t g_isr_sync_tick_count;
volatile uint32_t g_isr_sync_irq_give_count;
volatile uint32_t g_isr_sync_irq_queue_sent;
volatile uint32_t g_isr_sync_irq_queue_dropped;
volatile uint32_t g_isr_sync_sem_taken;
volatile uint32_t g_isr_sync_queue_received;
volatile uint32_t g_isr_sync_last_value;
volatile uint32_t g_isr_sync_error;
volatile uint32_t g_isr_sync_done;

void kernel_tick_isr_hook(void)
{
    uint32_t next_value;

    g_isr_sync_tick_count++;
    if ((g_isr_sync_tick_count % ISR_HOOK_PERIOD_TICKS) != 0U)
    {
        return;
    }

    next_value = g_isr_sync_irq_queue_sent + 1U;
    semaphore_give_from_isr(&isr_semaphore);
    g_isr_sync_irq_give_count++;

    if (queue_send_from_isr(&isr_queue, &next_value) != 0)
    {
        g_isr_sync_irq_queue_sent++;
    }
    else
    {
        g_isr_sync_irq_queue_dropped++;
    }
}

static void consumer_task(void *argument)
{
    uint32_t value;

    (void)argument;

    while (1)
    {
        if (semaphore_take(&isr_semaphore, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_isr_sync_error = 1U;
            continue;
        }
        g_isr_sync_sem_taken++;

        if (queue_receive(&isr_queue, &value, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_isr_sync_error = 2U;
            continue;
        }

        if ((g_isr_sync_last_value != 0U) && (value <= g_isr_sync_last_value))
        {
            g_isr_sync_error = 3U;
        }
        g_isr_sync_last_value = value;
        g_isr_sync_queue_received++;
    }
}

static void monitor_task(void *argument)
{
    (void)argument;

    while (1)
    {
        if (g_isr_sync_error != 0U)
        {
            while (1)
            {
                sleep_ticks(1U);
            }
        }

        if (g_isr_sync_queue_received >= ISR_EXPECTED_EVENTS)
        {
            if ((g_isr_sync_irq_queue_dropped != 0U)
                || (g_isr_sync_irq_queue_sent != g_isr_sync_queue_received)
                || (g_isr_sync_irq_give_count != g_isr_sync_sem_taken))
            {
                g_isr_sync_error = 4U;
            }
            g_isr_sync_done = 1U;
            while (1)
            {
                sleep_ticks(1U);
            }
        }

        sleep_ticks(1U);
    }
}

static const task_definition_t isr_sync_tasks[] = {
    { consumer_task, 0U, KERNEL_TASK_STACK_WORDS, 3U, "isr-consumer", 0U },
    { monitor_task, 0U, KERNEL_TASK_STACK_WORDS, 2U, "isr-monitor", 0U }
};

void isr_sync_paths_start(void)
{
    const kernel_config_t config = {
        isr_sync_tasks,
        sizeof(isr_sync_tasks) / sizeof(isr_sync_tasks[0])
    };

    semaphore_init(&isr_semaphore, 0U);
    queue_init(&isr_queue, isr_queue_storage, ISR_QUEUE_CAPACITY, sizeof(uint32_t));
    g_isr_sync_tick_count = 0U;
    g_isr_sync_irq_give_count = 0U;
    g_isr_sync_irq_queue_sent = 0U;
    g_isr_sync_irq_queue_dropped = 0U;
    g_isr_sync_sem_taken = 0U;
    g_isr_sync_queue_received = 0U;
    g_isr_sync_last_value = 0U;
    g_isr_sync_error = 0U;
    g_isr_sync_done = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
