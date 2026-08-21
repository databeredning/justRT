#include "kernel.h"
#include "../kernel/timer.h"
#include "../kernel/mempool.h"
#include "heartbeat.h"
#include "../board/board.h"

#define RUN_LED_PERIOD_MS 100U

volatile uint32_t g_periodic_delay_runs;
volatile uint32_t g_periodic_delay_last_tick;
volatile uint32_t g_timer_expirations;
volatile uint32_t g_timer_last_tick;
volatile uint32_t g_timer_callback_runs;
volatile uint32_t g_timer_callback_last_tick;
volatile uint32_t g_notification_sent;
volatile uint32_t g_notification_received;
volatile uint32_t g_notification_error;
volatile uint32_t g_event_group_waits;
volatile uint32_t g_event_group_error;
volatile uint32_t g_mempool_allocated;
volatile uint32_t g_mempool_reused;
volatile uint32_t g_mempool_error;
static event_group_t test_event_group;

static TASK_UNPRIVILEGED void heartbeat_task(void *argument)
{
    (void)argument;
    while (1)
    {
        board_led_toggle();
        sleep_ticks(ms_to_ticks(RUN_LED_PERIOD_MS));
    }
}

static TASK_UNPRIVILEGED void activity_task(void *argument)
{
    static uint32_t run_count;

    (void)argument;

    while (1)
    {
        run_count++;
        if ((run_count & 0xFFU) == 0U)
        {
            sleep_ticks(7U);
        }
    }
}

static TASK_UNPRIVILEGED void unprivileged_led_task(void *argument)
{
    (void)argument;
    while (1)
    {
        led_toggle();
        sleep_ticks(ms_to_ticks(RUN_LED_PERIOD_MS));
    }
}

static void periodic_delay_task(void *argument)
{
    uint32_t previous_wake = kernel_ticks_now();

    (void)argument;
    while (1)
    {
        task_delay_until(&previous_wake, ms_to_ticks(RUN_LED_PERIOD_MS));
        g_periodic_delay_runs++;
        g_periodic_delay_last_tick = kernel_ticks_now();
    }
}

static void timer_task(void *argument)
{
    static kernel_timer_t timer;

    (void)argument;
    kernel_timer_init(&timer);
    kernel_timer_start_periodic(&timer, ms_to_ticks(RUN_LED_PERIOD_MS));
    while (1)
    {
        uint32_t expirations = kernel_timer_take_expirations(&timer);

        g_timer_expirations += expirations;
        if (expirations != 0U)
        {
            g_timer_last_tick = kernel_ticks_now();
        }
        sleep_ticks(1U);
    }
}

static void timer_callback(void *argument)
{
    (void)argument;
    g_timer_callback_runs++;
    g_timer_callback_last_tick = kernel_ticks_now();
}

static void timer_callback_task(void *argument)
{
    static kernel_timer_t timer;

    (void)argument;
    kernel_timer_init(&timer);
    kernel_timer_set_callback(&timer, timer_callback, 0U);
    kernel_timer_start_periodic(&timer, ms_to_ticks(RUN_LED_PERIOD_MS));
    kernel_timer_stop(&timer);
    kernel_timer_restart(&timer);
    while (1)
    {
        kernel_timer_dispatch(&timer);
        sleep_ticks(1U);
    }
}

static void notification_producer_task(void *argument)
{
    (void)argument;
    while (1)
    {
        if (task_notify(1U, 1U) == 0)
        {
            g_notification_error++;
        }
        else
        {
            g_notification_sent++;
        }
        sleep_ticks(ms_to_ticks(RUN_LED_PERIOD_MS));
    }
}

static void notification_consumer_task(void *argument)
{
    uint32_t value;

    (void)argument;
    while (1)
    {
        if (task_notify_take(&value, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_notification_error++;
        }
        else
        {
            g_notification_received += value;
        }
    }
}

static void event_group_producer_task(void *argument)
{
    (void)argument;
    while (1)
    {
        event_group_set_bits(&test_event_group, 1U);
        sleep_ticks(1U);
        event_group_set_bits(&test_event_group, 2U);
        sleep_ticks(ms_to_ticks(RUN_LED_PERIOD_MS));
    }
}

static void event_group_consumer_task(void *argument)
{
    uint32_t result;

    (void)argument;
    while (1)
    {
        result = event_group_wait_bits(&test_event_group, 3U, 1, 1,
                                       SEMAPHORE_WAIT_FOREVER);
        if (result != 3U)
        {
            g_event_group_error++;
        }
        else
        {
            g_event_group_waits++;
        }
    }
}

static void mempool_task(void *argument)
{
    enum { POOL_BLOCKS = 4U, POOL_BLOCK_SIZE = 16U };
    static uint8_t storage[POOL_BLOCKS * POOL_BLOCK_SIZE]
        __attribute__((aligned(8)));
    static uint32_t used_bitmap[1];
    static memory_pool_t pool;
    void *blocks[POOL_BLOCKS];
    void *reused_block;
    uint32_t index;

    (void)argument;
    memory_pool_init(&pool, storage, POOL_BLOCK_SIZE, POOL_BLOCKS,
                     used_bitmap);
    while (1)
    {
        for (index = 0U; index < POOL_BLOCKS; index++)
        {
            blocks[index] = memory_pool_alloc(&pool);
            if (blocks[index] == 0U)
            {
                g_mempool_error++;
            }
            else
            {
                g_mempool_allocated++;
            }
        }
        if (memory_pool_alloc(&pool) != 0U)
        {
            g_mempool_error++;
        }
        if (memory_pool_free(&pool, (uint8_t *)blocks[0] + 1U) != 0)
        {
            g_mempool_error++;
        }
        for (index = 0U; index < POOL_BLOCKS; index++)
        {
            if (memory_pool_free(&pool, blocks[index]) == 0)
            {
                g_mempool_error++;
            }
        }
        if (memory_pool_free(&pool, blocks[0]) != 0)
        {
            g_mempool_error++;
        }
        reused_block = memory_pool_alloc(&pool);
        if (reused_block == 0U)
        {
            g_mempool_error++;
        }
        else
        {
            g_mempool_reused++;
            if (memory_pool_free(&pool, reused_block) == 0)
            {
                g_mempool_error++;
            }
        }
        sleep_ticks(ms_to_ticks(RUN_LED_PERIOD_MS));
    }
}

static const task_definition_t heartbeat_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { heartbeat_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "heartbeat", 0U },
    { activity_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "activity", 0U }
};

static const task_definition_t unprivileged_led_tasks[] TASK_UNPRIVILEGED_RODATA = {
        { unprivileged_led_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
            "unprivileged-led", TASK_FLAG_UNPRIVILEGED }
};

static const task_definition_t periodic_delay_tasks[] TASK_UNPRIVILEGED_RODATA = {
        { periodic_delay_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
            "periodic-delay", 0U }
};

static const task_definition_t timer_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { timer_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "timer-test", 0U }
};

static const task_definition_t timer_callback_tasks[] TASK_UNPRIVILEGED_RODATA = {
        { timer_callback_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
            "timer-callback", 0U }
};

static const task_definition_t notification_tasks[] TASK_UNPRIVILEGED_RODATA = {
        { notification_producer_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
            "notify-producer", 0U },
        { notification_consumer_task, 0U, KERNEL_TASK_STACK_WORDS, 2U,
            "notify-consumer", 0U }
};

static const task_definition_t event_group_tasks[] TASK_UNPRIVILEGED_RODATA = {
        { event_group_producer_task, 0U, KERNEL_TASK_STACK_WORDS, 1U,
            "event-producer", 0U },
        { event_group_consumer_task, 0U, KERNEL_TASK_STACK_WORDS, 2U,
            "event-consumer", 0U }
};

static const task_definition_t mempool_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { mempool_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "mempool-test", 0U }
};

void heartbeat_example_start(void)
{
    const kernel_config_t config = {
        heartbeat_tasks,
        sizeof(heartbeat_tasks) / sizeof(heartbeat_tasks[0])
    };

    board_init();
    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void heartbeat_unprivileged_led_start(void)
{
    const kernel_config_t config = {
        unprivileged_led_tasks,
        sizeof(unprivileged_led_tasks) / sizeof(unprivileged_led_tasks[0])
    };

    board_init();
    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void heartbeat_periodic_delay_start(void)
{
    const kernel_config_t config = {
        periodic_delay_tasks,
        sizeof(periodic_delay_tasks) / sizeof(periodic_delay_tasks[0])
    };

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void heartbeat_timer_start(void)
{
    const kernel_config_t config = {
        timer_tasks,
        sizeof(timer_tasks) / sizeof(timer_tasks[0])
    };

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void heartbeat_timer_callback_start(void)
{
    const kernel_config_t config = {
        timer_callback_tasks,
        sizeof(timer_callback_tasks) / sizeof(timer_callback_tasks[0])
    };

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void heartbeat_notification_start(void)
{
    const kernel_config_t config = {
        notification_tasks,
        sizeof(notification_tasks) / sizeof(notification_tasks[0])
    };

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void heartbeat_event_group_start(void)
{
    const kernel_config_t config = {
        event_group_tasks,
        sizeof(event_group_tasks) / sizeof(event_group_tasks[0])
    };

    event_group_init(&test_event_group);
    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void heartbeat_mempool_start(void)
{
    const kernel_config_t config = {
        mempool_tasks,
        sizeof(mempool_tasks) / sizeof(mempool_tasks[0])
    };

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
