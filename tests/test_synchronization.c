#include "kernel.h"
#include "sync.h"
#include "test_synchronization.h"

#define TEST_SYNC_QUEUE_CAPACITY 4U
#define TEST_SYNC_PERIOD_TICKS 4U
#define TEST_SYNC_TARGET 8U
#define TEST_SYNC_EVENT_BIT 1U

typedef struct
{
    volatile uint32_t tick_count;
    volatile uint32_t next_value;
} synchronization_test_isr_state_t;

static semaphore_t test_semaphore;
static queue_t test_queue;
static uint32_t test_queue_storage[TEST_SYNC_QUEUE_CAPACITY];
static event_group_t test_event_group;
static synchronization_test_isr_state_t test_isr_state;

synchronization_test_state_t g_test_synchronization;

static void test_synchronization_tick_hook(void)
{
    uint32_t value;

    test_isr_state.tick_count++;
    if ((test_isr_state.tick_count % TEST_SYNC_PERIOD_TICKS) != 0U
        || test_isr_state.next_value >= TEST_SYNC_TARGET)
    {
        return;
    }

    value = ++test_isr_state.next_value;
    semaphore_give_from_isr(&test_semaphore);
    if (queue_send_from_isr(&test_queue, &value) == 0)
    {
        g_test_synchronization.error_code = 1U;
    }
    if (event_group_set_bits_from_isr(&test_event_group,
                                      TEST_SYNC_EVENT_BIT) == 0U)
    {
        g_test_synchronization.error_code = 2U;
    }
    if (task_notify_from_isr(1U, 1U) == 0)
    {
        g_test_synchronization.error_code = 3U;
    }
}

static void test_sync_consumer_task(void *argument)
{
    uint32_t value;
    uint32_t last_value = 0U;

    (void)argument;
    while (g_test_synchronization.queue_received < TEST_SYNC_TARGET)
    {
        if (semaphore_take(&test_semaphore, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_test_synchronization.error_code = 4U;
            continue;
        }
        g_test_synchronization.semaphore_received++;

        if (queue_receive(&test_queue, &value, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_test_synchronization.error_code = 5U;
            continue;
        }
        if ((last_value != 0U) && (value != (last_value + 1U)))
        {
            g_test_synchronization.error_code = 6U;
        }
        last_value = value;
        g_test_synchronization.queue_received++;
    }

    while (1)
    {
        sleep_ticks(1U);
    }
}

static void test_sync_observer_task(void *argument)
{
    uint32_t event_value;
    uint32_t notification_value;

    (void)argument;
    while (g_test_synchronization.event_received < TEST_SYNC_TARGET
           || g_test_synchronization.notification_received < TEST_SYNC_TARGET)
    {
        event_value = event_group_wait_bits(&test_event_group,
                                            TEST_SYNC_EVENT_BIT, 0, 1,
                                            SEMAPHORE_WAIT_FOREVER);
        if ((event_value & TEST_SYNC_EVENT_BIT) == 0U)
        {
            g_test_synchronization.error_code = 7U;
        }
        else
        {
            g_test_synchronization.event_received++;
        }

        if (task_notify_take(&notification_value, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_test_synchronization.error_code = 8U;
        }
        else
        {
            g_test_synchronization.notification_received += notification_value;
        }
    }

    while (g_test_synchronization.queue_received < TEST_SYNC_TARGET)
    {
        yield();
    }
    if (g_test_synchronization.error_code == 0U
        && g_test_synchronization.semaphore_received >= TEST_SYNC_TARGET
        && g_test_synchronization.event_received >= TEST_SYNC_TARGET
        && g_test_synchronization.notification_received >= TEST_SYNC_TARGET)
    {
        g_test_synchronization.result.pass = 1U;
    }
    else
    {
        g_test_synchronization.result.fail = 1U;
    }
    g_test_synchronization.result.done = 1U;
    g_test_synchronization.result.state = TEST_STATE_COMPLETE;

    while (1)
    {
        sleep_ticks(1U);
    }
}

static const task_definition_t test_synchronization_tasks[] = {
    { test_sync_consumer_task, 0U, KERNEL_TASK_STACK_WORDS, 2U,
        "test-sync-consumer", 0U },
    { test_sync_observer_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
        "test-sync-observer", 0U }
};

void test_synchronization_start(void)
{
    const kernel_config_t config = {
        test_synchronization_tasks,
        sizeof(test_synchronization_tasks) / sizeof(test_synchronization_tasks[0])
    };

    g_test_synchronization.result.state = TEST_STATE_IDLE;
    g_test_synchronization.result.runs = 0U;
    g_test_synchronization.result.pass = 0U;
    g_test_synchronization.result.fail = 0U;
    g_test_synchronization.result.done = 0U;
    g_test_synchronization.semaphore_received = 0U;
    g_test_synchronization.queue_received = 0U;
    g_test_synchronization.event_received = 0U;
    g_test_synchronization.notification_received = 0U;
    g_test_synchronization.error_code = 0U;
    test_isr_state.tick_count = 0U;
    test_isr_state.next_value = 0U;

    semaphore_init(&test_semaphore, 0U);
    queue_init(&test_queue, test_queue_storage, TEST_SYNC_QUEUE_CAPACITY,
               sizeof(test_queue_storage[0]));
    event_group_init(&test_event_group);
    kernel_set_tick_hook(test_synchronization_tick_hook);

    if (kernel_init(&config) != KERNEL_OK)
    {
        g_test_synchronization.result.fail = 1U;
        return;
    }
    kernel_start();
}
