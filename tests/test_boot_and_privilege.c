#include "kernel.h"
#include "board/board.h"
#include "test_boot_and_privilege.h"

#define TEST_BOOT_ARGUMENT_MAGIC 0xB007A11DU
#define TEST_BOOT_RUN_TARGET 3U
#define TEST_BOOT_PERIOD_TICKS JRT_MillisecondsToTicks(100U)

test_result_t g_test_boot_and_privilege JRT_TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_test_boot_argument JRT_TASK_UNPRIVILEGED_DATA;

static JRT_TASK_UNPRIVILEGED void test_boot_task(void *argument)
{
    uint32_t argument_value = *(const uint32_t *)argument;

    g_test_boot_and_privilege.state = TEST_STATE_RUNNING;
    if (argument_value != TEST_BOOT_ARGUMENT_MAGIC)
    {
        g_test_boot_and_privilege.fail = 1U;
    }

    while (g_test_boot_and_privilege.runs < TEST_BOOT_RUN_TARGET)
    {
        JRT_BoardLedToggle();
        g_test_boot_and_privilege.runs++;
        JRT_TaskDelay(TEST_BOOT_PERIOD_TICKS);
    }

    if (g_test_boot_and_privilege.fail == 0U)
    {
        g_test_boot_and_privilege.pass = 1U;
    }
    g_test_boot_and_privilege.done = 1U;
    g_test_boot_and_privilege.state = TEST_STATE_COMPLETE;

    while (1)
    {
        JRT_TaskDelay(1U);
    }
}

JRT_DECLARE_STATIC_TASK_STACK(test_boot_stack, JRT_TASK_STACK_WORDS);

static const JRT_TaskDefinition_t test_tasks[] JRT_TASK_UNPRIVILEGED_RODATA = {
    JRT_TASK_DEFINITION(test_boot_task, (void *)&g_test_boot_argument, test_boot_stack, 1U, "test-boot", JRT_TASK_FLAG_UNPRIVILEGED)
};

void test_boot_and_privilege_start(void)
{
    const JRT_KernelConfig_t config = {
        test_tasks,
        sizeof(test_tasks) / sizeof(test_tasks[0])
    };

    g_test_boot_and_privilege.state = TEST_STATE_IDLE;
    g_test_boot_and_privilege.runs = 0U;
    g_test_boot_and_privilege.pass = 0U;
    g_test_boot_and_privilege.fail = 0U;
    g_test_boot_and_privilege.done = 0U;
    g_test_boot_argument = TEST_BOOT_ARGUMENT_MAGIC;

    board_init();
    if (JRT_KernelInit(&config) != JRT_STATUS_OK)
    {
        g_test_boot_and_privilege.fail = 1U;
        return;
    }
    JRT_KernelStart();
}
