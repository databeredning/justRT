#include "kernel.h"
#include "sync.h"
#include "test_race.h"

#define TEST_RACE_SEMAPHORE_ITERATIONS 4096U
#define TEST_RACE_QUEUE_ITERATIONS 1024U
#define TEST_RACE_QUEUE_SEND_ITERATIONS 512U
#define TEST_RACE_TIMEOUT_TICKS 3U
#define TEST_RACE_SIGNAL_SEMAPHORE 1U
#define TEST_RACE_SIGNAL_QUEUE 2U
#define TEST_RACE_SIGNAL_QUEUE_DRAIN 3U

static JRT_Semaphore_t test_race_semaphore;
static JRT_Queue_t test_race_queue;
static uint32_t test_race_queue_storage[1];
static JRT_Queue_t test_race_full_queue;
static uint32_t test_race_full_queue_storage[1];
static JRT_Semaphore_t test_race_drain_gate;
static volatile uint32_t test_race_signal_armed;
static volatile uint32_t test_race_signal_kind;

race_test_state_t g_test_race;

static void test_race_tick_hook(void)
{
    uint32_t value;

    if (test_race_signal_armed != 0U)
    {
        test_race_signal_armed = 0U;
        if (test_race_signal_kind == TEST_RACE_SIGNAL_SEMAPHORE)
        {
            g_test_race.signals++;
            JRT_SemaphoreGiveFromISR(&test_race_semaphore);
        }
        else if (test_race_signal_kind == TEST_RACE_SIGNAL_QUEUE)
        {
            value = g_test_race.queue_signals + 1U;
            if (JRT_QueueSendFromISR(&test_race_queue, &value) == 0)
            {
                g_test_race.error_code = 4U;
            }
            else
            {
                g_test_race.queue_signals++;
            }
        }
        else if (test_race_signal_kind == TEST_RACE_SIGNAL_QUEUE_DRAIN)
        {
            g_test_race.queue_drain_signals++;
            JRT_SemaphoreGiveFromISR(&test_race_drain_gate);
        }
    }
}

static void test_race_queue_drainer_task(void *argument)
{
    uint32_t value;

    (void)argument;
    while (1)
    {
        if (JRT_SemaphoreTake(&test_race_drain_gate,
                              JRT_WAIT_FOREVER) == 0)
        {
            g_test_race.error_code = 8U;
        }
        else if (JRT_QueueReceive(&test_race_full_queue, &value, 0U) == 0)
        {
            g_test_race.error_code = 9U;
        }
        else
        {
            g_test_race.queue_drains++;
        }
    }
}

static void test_race_waiter_task(void *argument)
{
    uint32_t iteration;
    uint32_t value;

    (void)argument;
    g_test_race.result.state = TEST_STATE_RUNNING;

    for (iteration = 0U; iteration < TEST_RACE_SEMAPHORE_ITERATIONS;
         iteration++)
    {
        test_race_signal_kind = TEST_RACE_SIGNAL_SEMAPHORE;
        test_race_signal_armed = 1U;
        if (JRT_SemaphoreTake(&test_race_semaphore,
                              TEST_RACE_TIMEOUT_TICKS) != 0)
        {
            g_test_race.takes++;
        }
        else
        {
            g_test_race.timeouts++;

            /* A token present immediately after a timeout identifies the
             * check-to-block lost-wakeup race rather than a missing signal. */
            if (JRT_SemaphoreTake(&test_race_semaphore, 0U) != 0)
            {
                g_test_race.lost_wakeups++;
                g_test_race.error_code = 1U;
            }
            else
            {
                g_test_race.error_code = 2U;
            }
        }
        g_test_race.result.runs++;
    }

    for (iteration = 0U; iteration < TEST_RACE_QUEUE_ITERATIONS; iteration++)
    {
        test_race_signal_kind = TEST_RACE_SIGNAL_QUEUE;
        test_race_signal_armed = 1U;
        if (JRT_QueueReceive(&test_race_queue, &value,
                             TEST_RACE_TIMEOUT_TICKS) != 0)
        {
            g_test_race.queue_receives++;
            if (value != (iteration + 1U))
            {
                g_test_race.error_code = 5U;
            }
        }
        else
        {
            g_test_race.queue_timeouts++;
            if (JRT_QueueReceive(&test_race_queue, &value, 0U) != 0)
            {
                g_test_race.queue_lost_wakeups++;
                g_test_race.error_code = 6U;
            }
            else
            {
                g_test_race.error_code = 7U;
            }
        }
        g_test_race.result.runs++;
    }

    for (iteration = 0U; iteration < TEST_RACE_QUEUE_SEND_ITERATIONS;
         iteration++)
    {
        value = iteration + 1U;
        test_race_signal_kind = TEST_RACE_SIGNAL_QUEUE_DRAIN;
        test_race_signal_armed = 1U;
        if (JRT_QueueSend(&test_race_full_queue, &value,
                          TEST_RACE_TIMEOUT_TICKS) != 0)
        {
            g_test_race.queue_sends++;
        }
        else
        {
            g_test_race.queue_send_timeouts++;
            if (JRT_QueueSend(&test_race_full_queue, &value, 0U) != 0)
            {
                g_test_race.queue_send_lost_wakeups++;
                g_test_race.error_code = 10U;
            }
            else
            {
                g_test_race.error_code = 11U;
            }
        }
        g_test_race.result.runs++;
    }

    if (g_test_race.error_code == 0U
        && g_test_race.signals == TEST_RACE_SEMAPHORE_ITERATIONS
        && g_test_race.takes == TEST_RACE_SEMAPHORE_ITERATIONS
        && g_test_race.queue_signals == TEST_RACE_QUEUE_ITERATIONS
        && g_test_race.queue_receives == TEST_RACE_QUEUE_ITERATIONS
        && g_test_race.queue_drain_signals
            == TEST_RACE_QUEUE_SEND_ITERATIONS
        && g_test_race.queue_drains == TEST_RACE_QUEUE_SEND_ITERATIONS
        && g_test_race.queue_sends == TEST_RACE_QUEUE_SEND_ITERATIONS)
    {
        g_test_race.result.pass = 1U;
    }
    else
    {
        g_test_race.result.fail = 1U;
    }
    g_test_race.result.done = 1U;
    g_test_race.result.state = TEST_STATE_COMPLETE;

    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

JRT_DECLARE_STATIC_TASK_STACK(test_race_waiter_stack, JRT_TASK_STACK_WORDS);
JRT_DECLARE_STATIC_TASK_STACK(test_race_queue_drainer_stack,
                              JRT_TASK_STACK_WORDS);

static const JRT_TaskDefinition_t test_race_tasks[] = {
    JRT_TASK_DEFINITION(test_race_queue_drainer_task, 0U,
                        test_race_queue_drainer_stack, 2U,
                        "test-race-queue-drainer", 0U),
    JRT_TASK_DEFINITION(test_race_waiter_task, 0U, test_race_waiter_stack,
                        1U, "test-race-waiter", 0U)
};

void test_race_start(void)
{
    uint32_t initial_value = 0U;
    const JRT_KernelConfig_t config = {
        test_race_tasks,
        sizeof(test_race_tasks) / sizeof(test_race_tasks[0])
    };

    g_test_race.result.state = TEST_STATE_IDLE;
    g_test_race.result.runs = 0U;
    g_test_race.result.pass = 0U;
    g_test_race.result.fail = 0U;
    g_test_race.result.done = 0U;
    g_test_race.signals = 0U;
    g_test_race.takes = 0U;
    g_test_race.timeouts = 0U;
    g_test_race.lost_wakeups = 0U;
    g_test_race.queue_signals = 0U;
    g_test_race.queue_receives = 0U;
    g_test_race.queue_timeouts = 0U;
    g_test_race.queue_lost_wakeups = 0U;
    g_test_race.queue_drain_signals = 0U;
    g_test_race.queue_drains = 0U;
    g_test_race.queue_sends = 0U;
    g_test_race.queue_send_timeouts = 0U;
    g_test_race.queue_send_lost_wakeups = 0U;
    g_test_race.error_code = 0U;
    test_race_signal_armed = 0U;
    test_race_signal_kind = 0U;

    JRT_SemaphoreCreateBinaryStatic(&test_race_semaphore, 0U);
    JRT_QueueCreateStatic(&test_race_queue, test_race_queue_storage,
                          sizeof(test_race_queue_storage)
                              / sizeof(test_race_queue_storage[0]),
                          sizeof(test_race_queue_storage[0]));
    JRT_QueueCreateStatic(&test_race_full_queue,
                          test_race_full_queue_storage,
                          sizeof(test_race_full_queue_storage)
                              / sizeof(test_race_full_queue_storage[0]),
                          sizeof(test_race_full_queue_storage[0]));
    JRT_SemaphoreCreateBinaryStatic(&test_race_drain_gate, 0U);
    if (JRT_QueueSend(&test_race_full_queue, &initial_value, 0U) == 0)
    {
        g_test_race.error_code = 12U;
        g_test_race.result.fail = 1U;
        return;
    }
    JRT_KernelSetTickHook(test_race_tick_hook);
    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_race.error_code = 3U;
        g_test_race.result.fail = 1U;
        return;
    }
    JRT_KernelStart();
}
