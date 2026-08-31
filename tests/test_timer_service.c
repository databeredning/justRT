#include "kernel.h"
#include "timer.h"
#include "test_timer_service.h"

#define TIMER_SERVICE_WAIT_LIMIT 20U
#define TIMER_SERVICE_TASK_PRIORITY 1U
#define TIMER_SERVICE_UNSET_TASK UINT32_MAX

static JRT_Timer_t one_shot_timer;
static JRT_Timer_t periodic_timer;
static JRT_Timer_t accumulated_timer;
static JRT_Timer_t restart_timer;
static JRT_Timer_t starter_timer;
static JRT_Timer_t target_timer;
static JRT_Timer_t polling_timer;
static JRT_Timer_t compatibility_timer;

timer_service_test_state_t g_test_timer_service;

static void record_error(uint32_t code)
{
    if (g_test_timer_service.error_code == 0U)
    {
        g_test_timer_service.error_code = code;
    }
}

static void check_service_context(void)
{
    uint32_t saved_critical;
    uint32_t task_index = task_current_index();

    if (kernel_in_isr() != 0)
    {
        record_error(1U);
    }
    saved_critical = critical_enter();
    if (saved_critical != 0U)
    {
        record_error(2U);
    }
    critical_exit(saved_critical);
    if (task_current_priority() != TIMER_SERVICE_TASK_PRIORITY)
    {
        record_error(3U);
    }
    if (g_test_timer_service.callback_task_index == TIMER_SERVICE_UNSET_TASK)
    {
        g_test_timer_service.callback_task_index = task_index;
    }
    else if (g_test_timer_service.callback_task_index != task_index)
    {
        record_error(4U);
    }
    g_test_timer_service.context_checks++;
}

static void one_shot_callback(void *argument)
{
    (void)argument;
    check_service_context();
    g_test_timer_service.one_shot_callbacks++;
}

static void periodic_callback(void *argument)
{
    (void)argument;
    check_service_context();
    g_test_timer_service.periodic_callbacks++;
    if (g_test_timer_service.periodic_callbacks == 3U)
    {
        JRT_TimerStop(&periodic_timer);
    }
}

static void accumulated_second_callback(void *argument)
{
    (void)argument;
    check_service_context();
    g_test_timer_service.accumulated_second_callbacks++;
}

static void accumulated_first_callback(void *argument)
{
    (void)argument;
    check_service_context();
    g_test_timer_service.accumulated_first_callbacks++;
    JRT_TimerSetCallback(&accumulated_timer, accumulated_second_callback, 0U);
}

static void restart_callback(void *argument)
{
    (void)argument;
    check_service_context();
    g_test_timer_service.restart_callbacks++;
    if (g_test_timer_service.restart_callbacks == 1U)
    {
        JRT_TimerRestart(&restart_timer);
    }
    else
    {
        JRT_TimerStop(&restart_timer);
    }
}

static void target_callback(void *argument)
{
    (void)argument;
    check_service_context();
    g_test_timer_service.target_callbacks++;
}

static void starter_callback(void *argument)
{
    (void)argument;
    check_service_context();
    g_test_timer_service.starter_callbacks++;
    JRT_TimerStart(&target_timer, 1U);
}

static void compatibility_callback(void *argument)
{
    (void)argument;
    g_test_timer_service.compatibility_callbacks++;
}

static int wait_for_value(volatile uint32_t *value, uint32_t expected)
{
    uint32_t waits;

    for (waits = 0U; waits < TIMER_SERVICE_WAIT_LIMIT; waits++)
    {
        if (*value >= expected)
        {
            return 1;
        }
        JRT_TaskDelay(1U);
    }
    return 0;
}

static void test_timer_service_task(void *argument)
{
    uint32_t saved_critical;

    (void)argument;
    g_test_timer_service.result.state = TEST_STATE_RUNNING;

    JRT_TimerStart(&one_shot_timer, 1U);
    if (wait_for_value(&g_test_timer_service.one_shot_callbacks, 1U) == 0)
    {
        record_error(10U);
    }
    g_test_timer_service.result.runs++;

    JRT_TimerStartPeriodic(&periodic_timer, 1U);
    if (wait_for_value(&g_test_timer_service.periodic_callbacks, 3U) == 0)
    {
        record_error(11U);
    }
    JRT_TaskDelay(2U);
    if (g_test_timer_service.periodic_callbacks != 3U)
    {
        record_error(12U);
    }
    g_test_timer_service.result.runs++;

    JRT_TimerStart(&polling_timer, 1U);
    JRT_TaskDelay(1U);
    g_test_timer_service.polling_expirations =
        JRT_TimerTakeExpirations(&polling_timer);
    if (g_test_timer_service.polling_expirations != 1U)
    {
        record_error(13U);
    }
    g_test_timer_service.result.runs++;

    JRT_TimerStartPeriodic(&accumulated_timer, 1U);
    JRT_TaskDelay(3U);
    JRT_TimerStop(&accumulated_timer);
    JRT_TimerSetCallback(&accumulated_timer, accumulated_first_callback, 0U);
    if (wait_for_value(&g_test_timer_service.accumulated_second_callbacks,
                       2U) == 0
        || g_test_timer_service.accumulated_first_callbacks != 1U)
    {
        record_error(14U);
    }
    g_test_timer_service.result.runs++;

    JRT_TimerStart(&restart_timer, 1U);
    if (wait_for_value(&g_test_timer_service.restart_callbacks, 2U) == 0)
    {
        record_error(15U);
    }
    JRT_TaskDelay(2U);
    if (g_test_timer_service.restart_callbacks != 2U)
    {
        record_error(16U);
    }
    g_test_timer_service.result.runs++;

    JRT_TimerStart(&starter_timer, 1U);
    if (wait_for_value(&g_test_timer_service.target_callbacks, 1U) == 0
        || g_test_timer_service.starter_callbacks != 1U)
    {
        record_error(17U);
    }
    g_test_timer_service.result.runs++;

    JRT_TimerStart(&one_shot_timer, 1U);
    JRT_TimerStart(&target_timer, 1U);
    if (wait_for_value(&g_test_timer_service.one_shot_callbacks, 2U) == 0
        || wait_for_value(&g_test_timer_service.target_callbacks, 2U) == 0)
    {
        record_error(18U);
    }
    g_test_timer_service.result.runs++;

    JRT_TimerStart(&compatibility_timer, 1U);
    JRT_TaskDelay(1U);
    saved_critical = critical_enter();
    JRT_TimerSetCallback(&compatibility_timer, compatibility_callback, 0U);
    JRT_TimerDispatch(&compatibility_timer);
    JRT_TimerSetCallback(&compatibility_timer, 0U, 0U);
    critical_exit(saved_critical);
    if (g_test_timer_service.compatibility_callbacks != 1U)
    {
        record_error(19U);
    }
    g_test_timer_service.result.runs++;

    if ((g_test_timer_service.error_code == 0U)
        && (g_test_timer_service.context_checks == 13U))
    {
        g_test_timer_service.result.pass = 1U;
    }
    else
    {
        g_test_timer_service.result.fail = 1U;
    }
    g_test_timer_service.result.state = TEST_STATE_COMPLETE;
    g_test_timer_service.result.done = 1U;

    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

JRT_DECLARE_STATIC_TASK_STACK(test_timer_service_stack, JRT_TASK_STACK_WORDS);

static const JRT_TaskDefinition_t test_timer_service_tasks[] = {
    JRT_TASK_DEFINITION(test_timer_service_task, 0U, test_timer_service_stack,
                        2U, "test-timer-service", 0U)
};

void test_timer_service_start(void)
{
    const JRT_KernelConfig_t config = {
        test_timer_service_tasks,
        sizeof(test_timer_service_tasks) / sizeof(test_timer_service_tasks[0])
    };

    g_test_timer_service.result.state = TEST_STATE_IDLE;
    g_test_timer_service.result.runs = 0U;
    g_test_timer_service.result.pass = 0U;
    g_test_timer_service.result.fail = 0U;
    g_test_timer_service.result.done = 0U;
    g_test_timer_service.one_shot_callbacks = 0U;
    g_test_timer_service.periodic_callbacks = 0U;
    g_test_timer_service.accumulated_first_callbacks = 0U;
    g_test_timer_service.accumulated_second_callbacks = 0U;
    g_test_timer_service.restart_callbacks = 0U;
    g_test_timer_service.starter_callbacks = 0U;
    g_test_timer_service.target_callbacks = 0U;
    g_test_timer_service.polling_expirations = 0U;
    g_test_timer_service.compatibility_callbacks = 0U;
    g_test_timer_service.context_checks = 0U;
    g_test_timer_service.callback_task_index = TIMER_SERVICE_UNSET_TASK;
    g_test_timer_service.error_code = 0U;

    JRT_TimerCreateStatic(&one_shot_timer);
    JRT_TimerSetCallback(&one_shot_timer, one_shot_callback, 0U);
    JRT_TimerCreateStatic(&periodic_timer);
    JRT_TimerSetCallback(&periodic_timer, periodic_callback, 0U);
    JRT_TimerCreateStatic(&accumulated_timer);
    JRT_TimerCreateStatic(&restart_timer);
    JRT_TimerSetCallback(&restart_timer, restart_callback, 0U);
    JRT_TimerCreateStatic(&starter_timer);
    JRT_TimerSetCallback(&starter_timer, starter_callback, 0U);
    JRT_TimerCreateStatic(&target_timer);
    JRT_TimerSetCallback(&target_timer, target_callback, 0U);
    JRT_TimerCreateStatic(&polling_timer);
    JRT_TimerCreateStatic(&compatibility_timer);

    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_timer_service.error_code = 20U;
        g_test_timer_service.result.fail = 1U;
        return;
    }
    JRT_KernelStart();
}
