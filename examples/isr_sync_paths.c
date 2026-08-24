#include <stdint.h>

#include "kernel.h"
#include "isr_sync_paths.h"
#include "board/board.h"

#define ISR_QUEUE_CAPACITY 8U
#define ISR_HOOK_PERIOD_TICKS 4U
#define ISR_EXPECTED_EVENTS 32U
#define ISR_QUEUE_FULL_EXPECTED_RECEIVES 48U
#define ISR_SOAK_EXPECTED_RECEIVES 128U
#define ISR_SOAK_LED_PERIOD_MS 100U

#define ISR_MODE_SYNC 0U
#define ISR_MODE_QUEUE_FULL 1U
#define ISR_MODE_SOAK 2U

static semaphore_t isr_semaphore;
static queue_t isr_queue;
static uint32_t isr_queue_storage[ISR_QUEUE_CAPACITY] TASK_UNPRIVILEGED_DATA;
static mutex_t soak_mutex;
static uint32_t isr_mode;

volatile uint32_t g_isr_sync_tick_count TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_sync_irq_give_count TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_sync_irq_queue_sent TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_sync_irq_queue_dropped TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_sync_sem_taken TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_sync_queue_received TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_sync_last_value TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_sync_error TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_sync_done TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_qfull_sent TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_qfull_received TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_qfull_dropped TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_qfull_error TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_qfull_done TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_soak_done TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_soak_error TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_soak_mutex_owner_loops TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_isr_soak_mutex_contender_loops TASK_UNPRIVILEGED_DATA;

static void isr_sync_tick_hook(void)
{
    uint32_t next_value;
    uint32_t period_ticks;

    g_isr_sync_tick_count++;
    period_ticks = ((isr_mode == ISR_MODE_QUEUE_FULL) || (isr_mode == ISR_MODE_SOAK))
        ? 1U
        : ISR_HOOK_PERIOD_TICKS;
    if ((g_isr_sync_tick_count % period_ticks) != 0U)
    {
        return;
    }

    if (isr_mode == ISR_MODE_QUEUE_FULL)
    {
        next_value = g_isr_qfull_sent + 1U;
    }
    else
    {
        next_value = g_isr_sync_irq_queue_sent + 1U;
    }
    semaphore_give_from_isr(&isr_semaphore);
    if (isr_mode == ISR_MODE_QUEUE_FULL)
    {
        g_isr_qfull_sent++;
    }
    else
    {
        g_isr_sync_irq_give_count++;
    }

    if (queue_send_from_isr(&isr_queue, &next_value) != 0)
    {
        if (isr_mode == ISR_MODE_QUEUE_FULL)
        {
            g_isr_sync_irq_queue_sent++;
        }
        else
        {
            g_isr_sync_irq_queue_sent++;
        }
    }
    else
    {
        if (isr_mode == ISR_MODE_QUEUE_FULL)
        {
            g_isr_qfull_dropped++;
        }
        else
        {
            g_isr_sync_irq_queue_dropped++;
        }
    }
}

static TASK_UNPRIVILEGED void consumer_task(void *argument)
{
    uint32_t value;

    (void)argument;

    while (1)
    {
        if (semaphore_take(&isr_semaphore, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_isr_sync_error = 1U;
            continue;
        }
        g_isr_sync_sem_taken++;

        if (queue_receive(&isr_queue, &value, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_isr_sync_error = 2U;
            continue;
        }

        if ((g_isr_sync_last_value != 0U) && (value <= g_isr_sync_last_value))
        {
            if (isr_mode == ISR_MODE_QUEUE_FULL)
            {
                g_isr_qfull_error = 3U;
            }
            else if (isr_mode == ISR_MODE_SOAK)
            {
                g_isr_soak_error = 3U;
            }
            else
            {
                g_isr_sync_error = 3U;
            }
        }
        g_isr_sync_last_value = value;
        if (isr_mode == ISR_MODE_QUEUE_FULL)
        {
            g_isr_qfull_received++;
            sleep_ticks(3U);
        }
        else if (isr_mode == ISR_MODE_SOAK)
        {
            g_isr_sync_queue_received++;
            sleep_ticks(2U);
        }
        else
        {
            g_isr_sync_queue_received++;
        }
    }
}

static TASK_UNPRIVILEGED void soak_heartbeat_task(void *argument)
{
    (void)argument;

    while (1)
    {
        board_led_toggle();
        sleep_ticks(ms_to_ticks(ISR_SOAK_LED_PERIOD_MS));
    }
}

static TASK_UNPRIVILEGED void soak_mutex_owner_task(void *argument)
{
    (void)argument;

    while (1)
    {
        if (mutex_lock(&soak_mutex, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_isr_soak_error = 10U;
            continue;
        }
        g_isr_soak_mutex_owner_loops++;
        sleep_ticks(2U);
        if (mutex_unlock(&soak_mutex) == 0)
        {
            g_isr_soak_error = 11U;
        }
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void soak_mutex_contender_task(void *argument)
{
    (void)argument;

    while (1)
    {
        if (mutex_lock(&soak_mutex, SEMAPHORE_WAIT_FOREVER) == 0)
        {
            g_isr_soak_error = 12U;
            continue;
        }
        g_isr_soak_mutex_contender_loops++;
        if (mutex_unlock(&soak_mutex) == 0)
        {
            g_isr_soak_error = 13U;
        }
        sleep_ticks(1U);
    }
}

static TASK_UNPRIVILEGED void monitor_task(void *argument)
{
    (void)argument;

    while (1)
    {
        if ((g_isr_sync_error != 0U) || (g_isr_qfull_error != 0U)
            || (g_isr_soak_error != 0U))
        {
            while (1)
            {
                sleep_ticks(1U);
            }
        }

        if ((isr_mode == ISR_MODE_SYNC)
            && (g_isr_sync_queue_received >= ISR_EXPECTED_EVENTS))
        {
            if ((g_isr_sync_irq_queue_dropped != 0U)
                || (g_isr_sync_irq_queue_sent != g_isr_sync_queue_received)
                || (g_isr_sync_irq_give_count != g_isr_sync_sem_taken))
            {
                g_isr_sync_error = 4U;
            }
            g_isr_sync_done = 1U;
            while (1)
            {
                sleep_ticks(1U);
            }
        }

        if ((isr_mode == ISR_MODE_QUEUE_FULL)
            && (g_isr_qfull_received >= ISR_QUEUE_FULL_EXPECTED_RECEIVES))
        {
            if ((g_isr_qfull_dropped == 0U)
                || (g_isr_sync_irq_queue_sent < g_isr_qfull_received)
                || (g_sync_context_misuse != 0U)
                || (g_sync_misuse_semaphore_take != 0U)
                || (g_sync_misuse_semaphore_give != 0U)
                || (g_sync_misuse_semaphore_give_from_isr != 0U)
                || (g_sync_misuse_mutex_lock != 0U)
                || (g_sync_misuse_mutex_unlock != 0U)
                || (g_sync_misuse_queue_send != 0U)
                || (g_sync_misuse_queue_receive != 0U)
                || (g_sync_misuse_queue_send_from_isr != 0U))
            {
                g_isr_qfull_error = 4U;
            }
            g_isr_qfull_done = 1U;
            while (1)
            {
                sleep_ticks(1U);
            }
        }

        if ((isr_mode == ISR_MODE_SOAK)
            && (g_isr_sync_queue_received >= ISR_SOAK_EXPECTED_RECEIVES))
        {
            if ((g_isr_queue_send_dropped == 0U)
                || (g_sync_context_misuse != 0U)
                || (g_isr_soak_mutex_owner_loops == 0U)
                || (g_isr_soak_mutex_contender_loops == 0U))
            {
                g_isr_soak_error = 4U;
            }
            g_isr_soak_done = 1U;
            while (1)
            {
                sleep_ticks(1U);
            }
        }

        sleep_ticks(1U);
    }
}

static const task_definition_t isr_sync_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { consumer_task, 0U, KERNEL_TASK_STACK_WORDS, 3U, "isr-consumer", 0U },
    { monitor_task, 0U, KERNEL_TASK_STACK_WORDS, 2U, "isr-monitor", 0U }
};

static const task_definition_t isr_soak_tasks[] TASK_UNPRIVILEGED_RODATA = {
    { consumer_task, 0U, KERNEL_TASK_STACK_WORDS, 4U, "soak-consumer", 0U },
    { monitor_task, 0U, KERNEL_TASK_STACK_WORDS, 3U, "soak-monitor", 0U },
    { soak_mutex_owner_task, 0U, KERNEL_TASK_STACK_WORDS, 2U, "soak-owner", 0U },
    { soak_mutex_contender_task, 0U, KERNEL_TASK_STACK_WORDS, 2U, "soak-contender", 0U },
    { soak_heartbeat_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "soak-heartbeat", 0U }
};

void isr_sync_paths_start(void)
{
    const kernel_config_t config = {
        isr_sync_tasks,
        sizeof(isr_sync_tasks) / sizeof(isr_sync_tasks[0])
    };

    isr_mode = ISR_MODE_SYNC;
    kernel_set_tick_hook(isr_sync_tick_hook);
    semaphore_init(&isr_semaphore, 0U);
    queue_init(&isr_queue, isr_queue_storage, ISR_QUEUE_CAPACITY, sizeof(uint32_t));
    g_isr_sync_tick_count = 0U;
    g_isr_sync_irq_give_count = 0U;
    g_isr_sync_irq_queue_sent = 0U;
    g_isr_sync_irq_queue_dropped = 0U;
    g_isr_sync_sem_taken = 0U;
    g_isr_sync_queue_received = 0U;
    g_isr_sync_last_value = 0U;
    g_isr_sync_error = 0U;
    g_isr_sync_done = 0U;
    g_isr_qfull_sent = 0U;
    g_isr_qfull_received = 0U;
    g_isr_qfull_dropped = 0U;
    g_isr_qfull_error = 0U;
    g_isr_qfull_done = 0U;
    g_isr_soak_done = 0U;
    g_isr_soak_error = 0U;
    g_isr_soak_mutex_owner_loops = 0U;
    g_isr_soak_mutex_contender_loops = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void isr_sync_queue_full_start(void)
{
    const kernel_config_t config = {
        isr_sync_tasks,
        sizeof(isr_sync_tasks) / sizeof(isr_sync_tasks[0])
    };

    isr_mode = ISR_MODE_QUEUE_FULL;
    kernel_set_tick_hook(isr_sync_tick_hook);
    semaphore_init(&isr_semaphore, 0U);
    queue_init(&isr_queue, isr_queue_storage, ISR_QUEUE_CAPACITY, sizeof(uint32_t));
    g_isr_sync_tick_count = 0U;
    g_isr_sync_irq_give_count = 0U;
    g_isr_sync_irq_queue_sent = 0U;
    g_isr_sync_irq_queue_dropped = 0U;
    g_isr_sync_sem_taken = 0U;
    g_isr_sync_queue_received = 0U;
    g_isr_sync_last_value = 0U;
    g_isr_sync_error = 0U;
    g_isr_sync_done = 0U;
    g_isr_qfull_sent = 0U;
    g_isr_qfull_received = 0U;
    g_isr_qfull_dropped = 0U;
    g_isr_qfull_error = 0U;
    g_isr_qfull_done = 0U;
    g_isr_soak_done = 0U;
    g_isr_soak_error = 0U;
    g_isr_soak_mutex_owner_loops = 0U;
    g_isr_soak_mutex_contender_loops = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}

void isr_sync_soak_start(void)
{
    const kernel_config_t config = {
        isr_soak_tasks,
        sizeof(isr_soak_tasks) / sizeof(isr_soak_tasks[0])
    };

    board_init();
    isr_mode = ISR_MODE_SOAK;
    kernel_set_tick_hook(isr_sync_tick_hook);
    semaphore_init(&isr_semaphore, 0U);
    queue_init(&isr_queue, isr_queue_storage, ISR_QUEUE_CAPACITY, sizeof(uint32_t));
    mutex_init(&soak_mutex);
    g_isr_sync_tick_count = 0U;
    g_isr_sync_irq_give_count = 0U;
    g_isr_sync_irq_queue_sent = 0U;
    g_isr_sync_irq_queue_dropped = 0U;
    g_isr_sync_sem_taken = 0U;
    g_isr_sync_queue_received = 0U;
    g_isr_sync_last_value = 0U;
    g_isr_sync_error = 0U;
    g_isr_sync_done = 0U;
    g_isr_qfull_sent = 0U;
    g_isr_qfull_received = 0U;
    g_isr_qfull_dropped = 0U;
    g_isr_qfull_error = 0U;
    g_isr_qfull_done = 0U;
    g_isr_soak_done = 0U;
    g_isr_soak_error = 0U;
    g_isr_soak_mutex_owner_loops = 0U;
    g_isr_soak_mutex_contender_loops = 0U;

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
