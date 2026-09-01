#include "kernel.h"
#include "test_task_suspension.h"

#define SUSPENSION_PRIVATE_SIZE 32U
#define SUSPENSION_PRIVATE_VALUE 0x51A7E123U
#define SUSPENSION_LOCAL_VALUE 0xC01DF00DU

typedef struct
{
    volatile uint32_t words[SUSPENSION_PRIVATE_SIZE / sizeof(uint32_t)];
} suspension_private_t;

static suspension_private_t suspension_private JRT_TASK_PRIVATE_DATA(SUSPENSION_PRIVATE_SIZE);
task_suspension_test_state_t g_test_task_suspension JRT_TASK_UNPRIVILEGED_DATA;
static volatile uint32_t suspension_phase JRT_TASK_UNPRIVILEGED_DATA;
static volatile uint32_t suspension_command JRT_TASK_UNPRIVILEGED_DATA;
static volatile uint32_t suspension_progress JRT_TASK_UNPRIVILEGED_DATA;
static volatile uint32_t suspension_isr_armed JRT_TASK_UNPRIVILEGED_DATA;
static volatile uint32_t suspension_blocker_ready JRT_TASK_UNPRIVILEGED_DATA;

static void record_error(uint32_t code)
{
    if (g_test_task_suspension.error_code == 0U)
    {
        g_test_task_suspension.error_code = code;
    }
}

#if defined(JUSTRT_TEST_TASK_SUSPENSION_MPU)

static JRT_TASK_UNPRIVILEGED void suspension_owner_task(void *argument)
{
    suspension_private_t *private_data = (suspension_private_t *)argument;

    private_data->words[0] = SUSPENSION_PRIVATE_VALUE;
    g_test_task_suspension.private_state_restored = 1U;
    g_test_task_suspension.self_suspend_entered = 1U;
    (void)JRT_TaskSuspend(JRT_TASK_ID_SELF);
    record_error(30U);
    while (1)
    {
    }
}

static JRT_TASK_UNPRIVILEGED void suspension_attacker_task(void *argument)
{
    (void)argument;

    if (g_test_task_suspension.self_suspend_entered == 0U)
    {
        record_error(31U);
    }
    g_test_task_suspension.cross_read_attempted = 1U;
    suspension_progress = suspension_private.words[0];
    record_error(32U);
    while (1)
    {
    }
}

#else

static void suspension_tick_hook(void)
{
    if (suspension_isr_armed != 0U)
    {
        suspension_isr_armed = 0U;
        g_test_task_suspension.isr_status = JRT_TaskSuspend(1U);
    }
}

static JRT_TASK_UNPRIVILEGED void suspension_worker_task(void *argument)
{
    suspension_private_t *private_data = (suspension_private_t *)argument;
    volatile uint32_t local_state = SUSPENSION_LOCAL_VALUE;

    private_data->words[0] = SUSPENSION_PRIVATE_VALUE;
    g_test_task_suspension.shared_accesses++;
    g_test_task_suspension.self_suspend_entered = 1U;
    if (JRT_TaskSuspend(JRT_TASK_ID_SELF) != JRT_STATUS_OK)
    {
        record_error(10U);
    }
    g_test_task_suspension.self_suspend_returned = 1U;
    if (local_state == SUSPENSION_LOCAL_VALUE)
    {
        g_test_task_suspension.stack_state_preserved = 1U;
    }
    if (private_data->words[0] == SUSPENSION_PRIVATE_VALUE)
    {
        g_test_task_suspension.private_state_restored = 1U;
    }

    suspension_phase = 2U;
    while (suspension_command != 2U)
    {
        suspension_progress++;
    }
    if ((local_state != SUSPENSION_LOCAL_VALUE)
        || (private_data->words[0] != SUSPENSION_PRIVATE_VALUE))
    {
        record_error(11U);
    }

    suspension_phase = 3U;
    JRT_TaskDelay(5U);
    suspension_phase = 4U;
    g_test_task_suspension.worker_complete = 1U;
    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

static void suspension_blocker_task(void *argument)
{
    uint32_t notification;

    (void)argument;
    suspension_blocker_ready = 1U;
    if (JRT_TaskNotifyTake(&notification, JRT_WAIT_FOREVER) == 0
        || notification != 0x55U)
    {
        record_error(12U);
    }
    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

static void suspension_controller_task(void *argument)
{
    uint32_t progress_before;
    uint32_t waits = 0U;

    (void)argument;
    suspension_isr_armed = 1U;
    JRT_KernelSetTickHook(suspension_tick_hook);
    JRT_TaskDelay(2U);
    JRT_KernelSetTickHook(0U);
    if (g_test_task_suspension.isr_status != JRT_STATUS_INVALID_CONTEXT)
    {
        record_error(1U);
    }

    while ((g_test_task_suspension.self_suspend_entered == 0U) && (waits < 20U))
    {
        waits++;
        JRT_TaskDelay(2U);
    }
    if (g_test_task_suspension.self_suspend_entered == 0U)
    {
        record_error(2U);
    }
    if (JRT_TaskSuspend(1U) == JRT_STATUS_INVALID_STATE)
    {
        g_test_task_suspension.suspend_again_rejected = 1U;
    }
    if (JRT_TaskResume(1U) == JRT_STATUS_OK)
    {
        g_test_task_suspension.resume_accepted = 1U;
    }
    if (JRT_TaskResume(1U) == JRT_STATUS_INVALID_STATE)
    {
        g_test_task_suspension.resume_again_rejected = 1U;
    }

    waits = 0U;
    while ((suspension_phase < 2U) && (waits < 40U))
    {
        waits++;
        JRT_TaskDelay(2U);
    }
    if (JRT_TaskSuspend(1U) == JRT_STATUS_OK)
    {
        g_test_task_suspension.suspend_other_accepted = 1U;
    }
    progress_before = suspension_progress;
    JRT_TaskDelay(2U);
    if (suspension_progress == progress_before)
    {
        g_test_task_suspension.suspended_progress_stable = 1U;
    }
    (void)JRT_TaskResume(1U);
    suspension_command = 2U;

    waits = 0U;
    while ((suspension_phase < 3U) && (waits < 20U))
    {
        waits++;
        JRT_TaskDelay(2U);
    }
    if (JRT_TaskSuspend(1U) == JRT_STATUS_INVALID_STATE)
    {
        g_test_task_suspension.sleeping_rejected = 1U;
    }

    waits = 0U;
    while ((suspension_blocker_ready == 0U) && (waits < 40U))
    {
        waits++;
        JRT_TaskDelay(2U);
    }
    if (JRT_TaskSuspend(2U) == JRT_STATUS_INVALID_STATE)
    {
        g_test_task_suspension.blocked_rejected = 1U;
    }
    (void)JRT_TaskNotify(2U, 0x55U);
    if (JRT_TaskSuspend(99U) == JRT_STATUS_INVALID_TASK)
    {
        g_test_task_suspension.invalid_id_rejected = 1U;
    }
    if (JRT_TaskSuspend(3U) == JRT_STATUS_INVALID_TASK)
    {
        g_test_task_suspension.internal_id_rejected = 1U;
    }
    if (JRT_TaskResume(JRT_TASK_ID_SELF) == JRT_STATUS_INVALID_TASK)
    {
        g_test_task_suspension.resume_self_rejected = 1U;
    }
    g_test_task_suspension.shared_accesses++;

    waits = 0U;
    while ((g_test_task_suspension.worker_complete == 0U) && (waits < 80U))
    {
        waits++;
        JRT_TaskDelay(2U);
    }
    if ((g_test_task_suspension.self_suspend_returned == 0U)
        || (g_test_task_suspension.suspend_again_rejected == 0U)
        || (g_test_task_suspension.resume_accepted == 0U)
        || (g_test_task_suspension.resume_again_rejected == 0U)
        || (g_test_task_suspension.suspend_other_accepted == 0U)
        || (g_test_task_suspension.suspended_progress_stable == 0U)
        || (g_test_task_suspension.sleeping_rejected == 0U)
        || (g_test_task_suspension.blocked_rejected == 0U)
        || (g_test_task_suspension.invalid_id_rejected == 0U)
        || (g_test_task_suspension.internal_id_rejected == 0U)
        || (g_test_task_suspension.resume_self_rejected == 0U)
        || (g_test_task_suspension.stack_state_preserved == 0U)
        || (g_test_task_suspension.private_state_restored == 0U)
        || (g_test_task_suspension.shared_accesses != 2U)
        || (g_test_task_suspension.worker_complete == 0U)
        || (g_test_task_suspension.error_code != 0U))
    {
        g_test_task_suspension.result.fail = 1U;
    }
    else
    {
        g_test_task_suspension.result.pass = 1U;
    }
    g_test_task_suspension.result.runs = 2U;
    g_test_task_suspension.result.state = TEST_STATE_COMPLETE;
    g_test_task_suspension.result.done = 1U;
    while (1)
    {
    }
}

#endif

JRT_DECLARE_STATIC_TASK_STACK(suspension_stack_a, JRT_TASK_STACK_WORDS);
JRT_DECLARE_STATIC_TASK_STACK(suspension_stack_b, JRT_TASK_STACK_WORDS);
#if !defined(JUSTRT_TEST_TASK_SUSPENSION_MPU)
JRT_DECLARE_STATIC_TASK_STACK(suspension_stack_c, JRT_TASK_STACK_WORDS);
#endif

#if defined(JUSTRT_TEST_TASK_SUSPENSION_MPU)
static const JRT_TaskDefinition_t suspension_tasks[] = {
    JRT_TASK_DEFINITION_WITH_PRIVATE_DATA(
        suspension_owner_task, &suspension_private, suspension_stack_a, 2U,
        "suspension-owner", JRT_TASK_FLAG_UNPRIVILEGED, suspension_private),
    JRT_TASK_DEFINITION(
        suspension_attacker_task, 0U, suspension_stack_b, 1U,
        "suspension-attacker", JRT_TASK_FLAG_UNPRIVILEGED)
};
#else
static const JRT_TaskDefinition_t suspension_tasks[] = {
    JRT_TASK_DEFINITION(
        suspension_controller_task, 0U, suspension_stack_a, 2U,
        "suspension-controller", 0U),
    JRT_TASK_DEFINITION_WITH_PRIVATE_DATA(
        suspension_worker_task, &suspension_private, suspension_stack_b, 1U,
        "suspension-worker", JRT_TASK_FLAG_UNPRIVILEGED, suspension_private),
    JRT_TASK_DEFINITION(
        suspension_blocker_task, 0U, suspension_stack_c, 1U,
        "suspension-blocker", 0U)
};
#endif

void test_task_suspension_start(void)
{
    const JRT_KernelConfig_t config = {
        suspension_tasks,
        sizeof(suspension_tasks) / sizeof(suspension_tasks[0])
    };
    volatile uint32_t *state_words = (volatile uint32_t *)&g_test_task_suspension;
    uint32_t index;

    for (index = 0U; index < (sizeof(g_test_task_suspension) / sizeof(uint32_t)); index++)
    {
        state_words[index] = 0U;
    }
    g_test_task_suspension.expected_fault_address = (uint32_t)(uintptr_t)&suspension_private.words[0];
    suspension_phase = 0U;
    suspension_command = 0U;
    suspension_progress = 0U;
    suspension_isr_armed = 0U;
    suspension_blocker_ready = 0U;

    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_task_suspension.error_code = 40U;
        g_test_task_suspension.result.fail = 1U;
        return;
    }
    g_test_task_suspension.result.state = TEST_STATE_RUNNING;
    JRT_KernelStart();
}
