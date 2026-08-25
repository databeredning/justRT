#include "kernel.h"
#include "board/board.h"
#include "test_boot_and_privilege.h"

#define TEST_BOOT_ARGUMENT_MAGIC 0xB007A11DU
#define TEST_BOOT_RUN_TARGET 3U
#define TEST_BOOT_PERIOD_TICKS ms_to_ticks(100U)

test_result_t g_test_boot_and_privilege TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_test_boot_argument TASK_UNPRIVILEGED_DATA;

static TASK_UNPRIVILEGED void test_boot_task(void *argument)
{
    uint32_t argument_value = *(const uint32_t *)argument;

    g_test_boot_and_privilege.state = TEST_STATE_RUNNING;
    if (argument_value != TEST_BOOT_ARGUMENT_MAGIC)
    {
        g_test_boot_and_privilege.fail = 1U;
    }

    while (g_test_boot_and_privilege.runs < TEST_BOOT_RUN_TARGET)
    {
        led_toggle();
        g_test_boot_and_privilege.runs++;
        sleep_ticks(TEST_BOOT_PERIOD_TICKS);
    }

    if (g_test_boot_and_privilege.fail == 0U)
    {
        g_test_boot_and_privilege.pass = 1U;
    }
    g_test_boot_and_privilege.done = 1U;
    g_test_boot_and_privilege.state = TEST_STATE_COMPLETE;

    while (1)
    {
        sleep_ticks(1U);
    }
}

static const task_definition_t test_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { test_boot_task, (void *)&g_test_boot_argument, KERNEL_TASK_STACK_WORDS,
        1U, "test-boot", TASK_FLAG_UNPRIVILEGED }
};

void test_boot_and_privilege_start(void)
{
    const kernel_config_t config = {
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
    if (kernel_init(&config) != KERNEL_OK)
    {
        g_test_boot_and_privilege.fail = 1U;
        return;
    }
    kernel_start();
}
