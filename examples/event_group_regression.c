#include <stdint.h>

#include "kernel.h"
#include "event_group_regression.h"
#include "../board/board.h"

/*
 * Each waiter owns a disjoint set of bits. Sharing bits between a wait-all
 * waiter that clears on exit and a wait-any waiter would let the higher
 * priority task consume the bits before the other re-checks them, which is
 * correct kernel behaviour but not a useful regression signal.
 */
#define EVENT_BIT_ALL_A 0x1U   /* wait-all task, cleared on exit */
#define EVENT_BIT_ALL_B 0x2U
#define EVENT_BIT_NEVER 0x4U   /* never set, drives the timeout path */
#define EVENT_BIT_ISR 0x8U     /* set from SysTick, cleared on exit */
#define EVENT_BIT_ANY_A 0x10U  /* wait-any task, cleared on exit */
#define EVENT_BIT_ANY_B 0x20U

#define EVENT_BITS_ALL (EVENT_BIT_ALL_A | EVENT_BIT_ALL_B)
#define EVENT_BITS_ANY (EVENT_BIT_ANY_A | EVENT_BIT_ANY_B)

#define EVENT_TIMEOUT_TICKS 30U
#define EVENT_ISR_PERIOD_TICKS 40U
#define EVENT_EXPECTED_ROUNDS 20U
#define EVENT_LED_PERIOD_MS 200U

static event_group_t regression_group;

volatile uint32_t g_event_regression_wait_any TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_event_regression_wait_all TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_event_regression_timeouts TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_event_regression_isr_sets TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_event_regression_isr_wakes TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_event_regression_clear_checks TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_event_regression_error TASK_UNPRIVILEGED_DATA;
volatile uint32_t g_event_regression_done TASK_UNPRIVILEGED_DATA;

static volatile uint32_t regression_tick_count TASK_UNPRIVILEGED_DATA;

/*
 * Sets EVENT_BIT_ISR from SysTick context, exercising the ISR wake path of
 * event_group_set_bits_from_isr().
 */
static void regression_tick_hook(void)
{
    regression_tick_count++;
    if ((regression_tick_count % EVENT_ISR_PERIOD_TICKS) != 0U)
    {
        return;
    }

    if (event_group_set_bits_from_isr(&regression_group, EVENT_BIT_ISR) != 0U)
    {
        g_event_regression_isr_sets++;
    }
}

/*
 * Blocks on a bit no producer ever sets, so every wait must expire and return
 * zero without corrupting the group.
 */
static void timeout_task(void *argument)
{
    uint32_t result;

    (void)argument;

    while (1)
    {
        result = event_group_wait_bits(&regression_group, EVENT_BIT_NEVER, 0, 0,
                                       EVENT_TIMEOUT_TICKS);
        if (result != 0U)
        {
            g_event_regression_error = 1U;
        }
        else
        {
            g_event_regression_timeouts++;
        }

        if ((event_group_get_bits(&regression_group) & EVENT_BIT_NEVER) != 0U)
        {
            g_event_regression_error = 2U;
        }
    }
}

/* Waits for the SysTick-set bit and clears it on exit. */
static void isr_wait_task(void *argument)
{
    uint32_t result;

    (void)argument;

    while (1)
    {
        result = event_group_wait_bits(&regression_group, EVENT_BIT_ISR, 0, 1,
                                       SEMAPHORE_WAIT_FOREVER);
        if ((result & EVENT_BIT_ISR) == 0U)
        {
            g_event_regression_error = 3U;
            continue;
        }
        g_event_regression_isr_wakes++;

        /* clear_on_exit must have consumed the bit before we resume. */
        if ((event_group_get_bits(&regression_group) & EVENT_BIT_ISR) != 0U)
        {
            g_event_regression_error = 4U;
        }
        else
        {
            g_event_regression_clear_checks++;
        }
    }
}

static void producer_task(void *argument)
{
    (void)argument;

    while (1)
    {
        event_group_set_bits(&regression_group, EVENT_BIT_ALL_A);
        sleep_ticks(2U);
        event_group_set_bits(&regression_group, EVENT_BIT_ALL_B);
        event_group_set_bits(&regression_group, EVENT_BIT_ANY_A);
        sleep_ticks(8U);
    }
}

/* Wait-any: a single owned bit satisfies the wait, both are cleared on exit. */
static void wait_any_task(void *argument)
{
    uint32_t result;

    (void)argument;

    while (1)
    {
        result = event_group_wait_bits(&regression_group, EVENT_BITS_ANY, 0, 1,
                                       SEMAPHORE_WAIT_FOREVER);
        if ((result & EVENT_BITS_ANY) == 0U)
        {
            g_event_regression_error = 5U;
            continue;
        }
        g_event_regression_wait_any++;

        if ((event_group_get_bits(&regression_group) & EVENT_BITS_ANY) != 0U)
        {
            g_event_regression_error = 8U;
        }
    }
}

/* Wait-all: both bits must be present, then both are cleared on exit. */
static void wait_all_task(void *argument)
{
    uint32_t result;

    (void)argument;

    while (1)
    {
        result = event_group_wait_bits(&regression_group, EVENT_BITS_ALL, 1, 1,
                                       SEMAPHORE_WAIT_FOREVER);
        if ((result & EVENT_BITS_ALL) != EVENT_BITS_ALL)
        {
            g_event_regression_error = 6U;
            continue;
        }
        g_event_regression_wait_all++;

        if ((event_group_get_bits(&regression_group) & EVENT_BITS_ALL) != 0U)
        {
            g_event_regression_error = 7U;
        }

        if ((g_event_regression_wait_all >= EVENT_EXPECTED_ROUNDS)
            && (g_event_regression_timeouts > 0U)
            && (g_event_regression_isr_wakes > 0U)
            && (g_event_regression_error == 0U))
        {
            g_event_regression_done = 1U;
        }
    }
}

static void heartbeat_task(void *argument)
{
    (void)argument;

    while (1)
    {
        board_led_toggle();
        sleep_ticks(ms_to_ticks(EVENT_LED_PERIOD_MS));
    }
}

static const task_definition_t event_regression_tasks[] = {
    { producer_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "evt-producer", 0U },
    { wait_all_task, 0U, KERNEL_TASK_STACK_WORDS, 3U, "evt-wait-all", 0U },
    { wait_any_task, 0U, KERNEL_TASK_STACK_WORDS, 2U, "evt-wait-any", 0U },
    { timeout_task, 0U, KERNEL_TASK_STACK_WORDS, 2U, "evt-timeout", 0U },
    { isr_wait_task, 0U, KERNEL_TASK_STACK_WORDS, 2U, "evt-isr-wait", 0U },
    { heartbeat_task, 0U, KERNEL_TASK_STACK_WORDS, 1U, "evt-led", 0U }
};

void event_group_regression_start(void)
{
    const kernel_config_t config = {
        event_regression_tasks,
        sizeof(event_regression_tasks) / sizeof(event_regression_tasks[0])
    };

    board_init();
    event_group_init(&regression_group);
    regression_tick_count = 0U;
    g_event_regression_wait_any = 0U;
    g_event_regression_wait_all = 0U;
    g_event_regression_timeouts = 0U;
    g_event_regression_isr_sets = 0U;
    g_event_regression_isr_wakes = 0U;
    g_event_regression_clear_checks = 0U;
    g_event_regression_error = 0U;
    g_event_regression_done = 0U;
    kernel_set_tick_hook(regression_tick_hook);

    if (kernel_init(&config) != KERNEL_OK)
    {
        while (1)
        {
        }
    }
    kernel_start();
}
