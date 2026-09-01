#include "kernel.h"
#include "test_mpu_isolation.h"

#define MPU_ISOLATION_REGION_SIZE 32U
#define MPU_ISOLATION_VALUE_A 0xA51A51A5U
#define MPU_ISOLATION_VALUE_B 0xB62B62B6U
#define MPU_ISOLATION_SHARED_VALUE 0x5A6E3D21U

typedef struct
{
    volatile uint32_t words[MPU_ISOLATION_REGION_SIZE / sizeof(uint32_t)];
} mpu_isolation_region_t;

static mpu_isolation_region_t mpu_isolation_private_a JRT_TASK_PRIVATE_DATA(MPU_ISOLATION_REGION_SIZE);
static mpu_isolation_region_t mpu_isolation_private_b JRT_TASK_PRIVATE_DATA(MPU_ISOLATION_REGION_SIZE);

mpu_isolation_test_state_t g_test_mpu_isolation JRT_TASK_UNPRIVILEGED_DATA;
static volatile uint32_t mpu_isolation_shared_value JRT_TASK_UNPRIVILEGED_DATA;
static volatile uint32_t mpu_isolation_read_sink JRT_TASK_UNPRIVILEGED_DATA;

static void record_error(uint32_t code)
{
    if (g_test_mpu_isolation.error_code == 0U)
    {
        g_test_mpu_isolation.error_code = code;
    }
}

static JRT_TASK_UNPRIVILEGED void mpu_isolation_task_a(void *argument)
{
    mpu_isolation_region_t *private_data = (mpu_isolation_region_t *)argument;

    private_data->words[0] = MPU_ISOLATION_VALUE_A;
    if (private_data->words[0] == MPU_ISOLATION_VALUE_A)
    {
        g_test_mpu_isolation.task_a_private_access = 1U;
    }
    else
    {
        record_error(1U);
    }

    mpu_isolation_shared_value = MPU_ISOLATION_SHARED_VALUE;
    if (mpu_isolation_shared_value == MPU_ISOLATION_SHARED_VALUE)
    {
        g_test_mpu_isolation.shared_accesses = 1U;
    }
    else
    {
        record_error(2U);
    }

    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

static JRT_TASK_UNPRIVILEGED void mpu_isolation_task_b(void *argument)
{
    mpu_isolation_region_t *private_data = (mpu_isolation_region_t *)argument;

    private_data->words[0] = MPU_ISOLATION_VALUE_B;
    if (private_data->words[0] == MPU_ISOLATION_VALUE_B)
    {
        g_test_mpu_isolation.task_b_private_access = 1U;
    }
    else
    {
        record_error(3U);
    }

    if (mpu_isolation_shared_value == MPU_ISOLATION_SHARED_VALUE)
    {
        g_test_mpu_isolation.shared_accesses++;
    }
    else
    {
        record_error(4U);
    }

    if ((g_test_mpu_isolation.task_a_private_access != 0U)
        && (g_test_mpu_isolation.task_b_private_access != 0U))
    {
        g_test_mpu_isolation.context_switch_revoked_access = 1U;
    }
    else
    {
        record_error(5U);
    }

    if ((g_test_mpu_isolation.task_a_private_access == 0U)
        || (g_test_mpu_isolation.task_b_private_access == 0U)
        || (g_test_mpu_isolation.shared_accesses != 2U)
        || (g_test_mpu_isolation.context_switch_revoked_access == 0U)
        || (g_test_mpu_isolation.error_code != 0U))
    {
        g_test_mpu_isolation.result.fail = 1U;
        g_test_mpu_isolation.result.done = 1U;
        while (1)
        {
        }
    }

    g_test_mpu_isolation.result.runs = 2U;
#if defined(JUSTRT_TEST_MPU_ISOLATION_WRITE)
    g_test_mpu_isolation.cross_write_attempted = 1U;
    mpu_isolation_private_a.words[0] = 0xBAD00BADU;
#else
    g_test_mpu_isolation.cross_read_attempted = 1U;
    mpu_isolation_read_sink = mpu_isolation_private_a.words[0];
#endif

    record_error(6U);
    g_test_mpu_isolation.result.fail = 1U;
    g_test_mpu_isolation.result.done = 1U;
    while (1)
    {
    }
}

JRT_DECLARE_STATIC_TASK_STACK(mpu_isolation_stack_a, JRT_TASK_STACK_WORDS);
JRT_DECLARE_STATIC_TASK_STACK(mpu_isolation_stack_b, JRT_TASK_STACK_WORDS);

static const JRT_TaskDefinition_t mpu_isolation_tasks[] = {
    JRT_TASK_DEFINITION_WITH_PRIVATE_DATA(
        mpu_isolation_task_a, &mpu_isolation_private_a, mpu_isolation_stack_a,
        2U, "mpu-isolation-a", JRT_TASK_FLAG_UNPRIVILEGED,
        mpu_isolation_private_a),
    JRT_TASK_DEFINITION_WITH_PRIVATE_DATA(
        mpu_isolation_task_b, &mpu_isolation_private_b, mpu_isolation_stack_b,
        1U, "mpu-isolation-b", JRT_TASK_FLAG_UNPRIVILEGED,
        mpu_isolation_private_b)
};

void test_mpu_isolation_start(void)
{
    const JRT_KernelConfig_t config = {
        mpu_isolation_tasks,
        sizeof(mpu_isolation_tasks) / sizeof(mpu_isolation_tasks[0])
    };

    g_test_mpu_isolation.result.state = TEST_STATE_IDLE;
    g_test_mpu_isolation.result.runs = 0U;
    g_test_mpu_isolation.result.pass = 0U;
    g_test_mpu_isolation.result.fail = 0U;
    g_test_mpu_isolation.result.done = 0U;
    g_test_mpu_isolation.expected_fault_address = (uint32_t)(uintptr_t)&mpu_isolation_private_a.words[0];
    g_test_mpu_isolation.task_a_private_access = 0U;
    g_test_mpu_isolation.task_b_private_access = 0U;
    g_test_mpu_isolation.shared_accesses = 0U;
    g_test_mpu_isolation.context_switch_revoked_access = 0U;
    g_test_mpu_isolation.cross_read_attempted = 0U;
    g_test_mpu_isolation.cross_write_attempted = 0U;
    g_test_mpu_isolation.error_code = 0U;
    mpu_isolation_shared_value = 0U;
    mpu_isolation_read_sink = 0U;

    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_mpu_isolation.error_code = 7U;
        g_test_mpu_isolation.result.fail = 1U;
        return;
    }
    g_test_mpu_isolation.result.state = TEST_STATE_RUNNING;
    JRT_KernelStart();
}
