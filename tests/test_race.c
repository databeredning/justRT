#include "kernel.h"
#include "sync.h"
#include "test_race.h"

#define TEST_RACE_ITERATIONS 4096U
#define TEST_RACE_TIMEOUT_TICKS 3U

static JRT_Semaphore_t test_race_semaphore;
static volatile uint32_t test_race_signal_armed;

race_test_state_t g_test_race;

static void test_race_tick_hook(void)
{
    if (test_race_signal_armed != 0U)
    {
        test_race_signal_armed = 0U;
        g_test_race.signals++;
        JRT_SemaphoreGiveFromISR(&test_race_semaphore);
    }
}

static void test_race_waiter_task(void *argument)
{
    uint32_t iteration;

    (void)argument;
    g_test_race.result.state = TEST_STATE_RUNNING;

    for (iteration = 0U; iteration < TEST_RACE_ITERATIONS; iteration++)
    {
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

    if (g_test_race.error_code == 0U
        && g_test_race.signals == TEST_RACE_ITERATIONS
        && g_test_race.takes == TEST_RACE_ITERATIONS)
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

static const JRT_TaskDefinition_t test_race_tasks[] = {
    JRT_TASK_DEFINITION(test_race_waiter_task, 0U, test_race_waiter_stack,
                        1U, "test-race-waiter", 0U)
};

void test_race_start(void)
{
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
    g_test_race.error_code = 0U;
    test_race_signal_armed = 0U;

    JRT_SemaphoreCreateBinaryStatic(&test_race_semaphore, 0U);
    JRT_KernelSetTickHook(test_race_tick_hook);
    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_race.error_code = 3U;
        g_test_race.result.fail = 1U;
        return;
    }
    JRT_KernelStart();
}
