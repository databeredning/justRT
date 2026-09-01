#include "kernel.h"
#include "test_task_capacity.h"

#define TASK_CAPACITY_WAIT_LIMIT 100U

typedef struct __attribute__((aligned(32)))
{
    uint32_t guard[JRT_TASK_GUARD_WORDS];
    uint32_t stack[JRT_TASK_STACK_WORDS];
} task_capacity_storage_t;

static task_capacity_storage_t
    task_capacity_storage[JRT_MAX_APPLICATION_TASKS + 1U];
static JRT_TaskDefinition_t
    task_capacity_definitions[JRT_MAX_APPLICATION_TASKS + 1U];
static uint32_t task_capacity_indices[JRT_MAX_APPLICATION_TASKS];
static volatile uint32_t task_capacity_ran[JRT_MAX_APPLICATION_TASKS];

task_capacity_test_state_t g_test_task_capacity;

static void record_error(uint32_t code)
{
    if (g_test_task_capacity.error_code == 0U)
    {
        g_test_task_capacity.error_code = code;
    }
}

static void task_capacity_task(void *argument)
{
    uint32_t index = *(const uint32_t *)argument;
    uint32_t waits = 0U;

    if (index >= JRT_MAX_APPLICATION_TASKS)
    {
        record_error(1U);
    }
    else
    {
        task_capacity_ran[index] = 1U;
    }

    while (1)
    {
        if (index == 0U)
        {
            uint32_t scan;
            uint32_t count = 0U;

            for (scan = 0U; scan < JRT_MAX_APPLICATION_TASKS; scan++)
            {
                count += task_capacity_ran[scan];
            }
            g_test_task_capacity.tasks_ran = count;
            if (count == JRT_MAX_APPLICATION_TASKS)
            {
                g_test_task_capacity.guard_updates =
                    g_mpu_stack_guard_updates;
                g_test_task_capacity.guard_base_matches =
                    (g_mpu_stack_guard_base
                     == (uint32_t)(uintptr_t)&task_capacity_storage[0].guard[0])
                        ? 1U : 0U;
                g_test_task_capacity.ready_scan_depth =
                    g_ready_scan_depth_max;
                g_test_task_capacity.scheduler_pass2_max =
                    g_sched_pass2_iters_max;
                if (g_test_task_capacity.guard_updates
                        < JRT_MAX_APPLICATION_TASKS
                    || g_test_task_capacity.guard_base_matches == 0U)
                {
                    record_error(2U);
                }
                if (g_test_task_capacity.error_code == 0U)
                {
                    g_test_task_capacity.result.pass = 1U;
                }
                else
                {
                    g_test_task_capacity.result.fail = 1U;
                }
                g_test_task_capacity.result.runs = count;
                g_test_task_capacity.result.state = TEST_STATE_COMPLETE;
                g_test_task_capacity.result.done = 1U;
            }
            else if (++waits >= TASK_CAPACITY_WAIT_LIMIT)
            {
                record_error(3U);
                g_test_task_capacity.result.fail = 1U;
                g_test_task_capacity.result.state = TEST_STATE_COMPLETE;
                g_test_task_capacity.result.done = 1U;
            }
        }
        JRT_TaskDelay(1U);
    }
}

static void prepare_definitions(void)
{
    uint32_t index;

    for (index = 0U; index <= JRT_MAX_APPLICATION_TASKS; index++)
    {
        JRT_TaskDefinition_t *definition =
            &task_capacity_definitions[index];

        definition->entry = task_capacity_task;
        definition->argument = (index < JRT_MAX_APPLICATION_TASKS)
            ? (void *)&task_capacity_indices[index] : 0U;
        definition->stack_buffer = &task_capacity_storage[index].stack[0];
        definition->stack_words = JRT_TASK_STACK_WORDS;
        definition->stack_guard = &task_capacity_storage[index].guard[0];
        definition->priority = 1U;
        definition->name = "test-task-capacity";
        definition->flags = 0U;
        definition->private_data_base = 0U;
        definition->private_data_size = 0U;
    }
}

void test_task_capacity_start(void)
{
    JRT_KernelConfig_t maximum_config = {
        task_capacity_definitions,
        JRT_MAX_APPLICATION_TASKS
    };
    JRT_KernelConfig_t too_many_config = {
        task_capacity_definitions,
        JRT_MAX_APPLICATION_TASKS + 1U
    };
    uint32_t index;

    g_test_task_capacity.result.state = TEST_STATE_IDLE;
    g_test_task_capacity.result.runs = 0U;
    g_test_task_capacity.result.pass = 0U;
    g_test_task_capacity.result.fail = 0U;
    g_test_task_capacity.result.done = 0U;
    g_test_task_capacity.configured_limit = JRT_MAX_APPLICATION_TASKS;
    g_test_task_capacity.tasks_ran = 0U;
    g_test_task_capacity.maximum_accepted = 0U;
    g_test_task_capacity.maximum_plus_one_rejected = 0U;
    g_test_task_capacity.guard_updates = 0U;
    g_test_task_capacity.guard_base_matches = 0U;
    g_test_task_capacity.ready_scan_depth = 0U;
    g_test_task_capacity.scheduler_pass2_max = 0U;
    g_test_task_capacity.error_code = 0U;
    for (index = 0U; index < JRT_MAX_APPLICATION_TASKS; index++)
    {
        task_capacity_indices[index] = index;
        task_capacity_ran[index] = 0U;
    }
    prepare_definitions();

    if (JRT_KernelInit(&too_many_config) == JRT_STATUS_TOO_MANY_TASKS)
    {
        g_test_task_capacity.maximum_plus_one_rejected = 1U;
    }
    else
    {
        record_error(10U);
    }
    if (JRT_KernelInit(&maximum_config) == JRT_STATUS_OK)
    {
        g_test_task_capacity.maximum_accepted = 1U;
    }
    else
    {
        record_error(11U);
        g_test_task_capacity.result.fail = 1U;
        return;
    }
    g_test_task_capacity.result.state = TEST_STATE_RUNNING;
    JRT_KernelStart();
}
