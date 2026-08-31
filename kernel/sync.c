#include "kernel.h"
#include "sync.h"
#include "cortex_m/port_contract.h"

volatile uint32_t g_sync_context_misuse KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_sync_misuse_semaphore_take KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_sync_misuse_semaphore_give KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_sync_misuse_semaphore_give_from_isr KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_sync_misuse_mutex_lock KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_sync_misuse_mutex_unlock KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_sync_misuse_queue_send KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_sync_misuse_queue_receive KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_sync_misuse_queue_send_from_isr KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_isr_queue_send_attempted KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_isr_queue_send_accepted KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_isr_queue_send_dropped KERNEL_PRIVILEGED_DATA;
volatile uint32_t g_isr_queue_count_high_water KERNEL_PRIVILEGED_DATA;

static void count_context_misuse(volatile uint32_t *counter)
{
    (*counter)++;
    g_sync_context_misuse++;
}

void JRT_SemaphoreCreateBinaryStatic(JRT_Semaphore_t *semaphore,
                                     uint32_t initially_available)
{
    uint32_t saved_primask = arch_critical_enter();

    semaphore->available = (initially_available != 0U) ? 1U : 0U;
    arch_critical_exit(saved_primask);
}

int JRT_SemaphoreTake(JRT_Semaphore_t *semaphore, uint32_t timeout_ticks)
{
    uint32_t saved_primask;

    if (arch_in_isr() != 0)
    {
        count_context_misuse(&g_sync_misuse_semaphore_take);
        return 0;
    }

    saved_primask = arch_critical_enter();
    if (semaphore->available != 0U)
    {
        semaphore->available = 0U;
        arch_critical_exit(saved_primask);
        return 1;
    }

    if (timeout_ticks == 0U)
    {
        arch_critical_exit(saved_primask);
        return 0;
    }

    return task_block_locked(semaphore, TASK_WAIT_SEMAPHORE, timeout_ticks,
                             saved_primask);
}

void JRT_SemaphoreGive(JRT_Semaphore_t *semaphore)
{
    if (arch_in_isr() != 0)
    {
        count_context_misuse(&g_sync_misuse_semaphore_give);
        return;
    }

    uint32_t saved_primask = arch_critical_enter();

    if (task_wake(semaphore, TASK_WAIT_SEMAPHORE) == 0)
    {
        semaphore->available = 1U;
    }
    arch_critical_exit(saved_primask);
}

void JRT_SemaphoreGiveFromISR(JRT_Semaphore_t *semaphore)
{
    if (arch_in_isr() == 0)
    {
        count_context_misuse(&g_sync_misuse_semaphore_give_from_isr);
        return;
    }

    uint32_t saved_primask = arch_critical_enter();

    if (task_wake(semaphore, TASK_WAIT_SEMAPHORE) == 0)
    {
        semaphore->available = 1U;
    }
    arch_critical_exit(saved_primask);
    arch_request_switch();
}

void JRT_MutexCreateRecursiveStatic(JRT_Mutex_t *mutex)
{
    uint32_t saved_primask = arch_critical_enter();

    mutex->locked = 0U;
    mutex->owner = UINT32_MAX;
    mutex->recursion = 0U;
    arch_critical_exit(saved_primask);
}

int JRT_MutexLock(JRT_Mutex_t *mutex, uint32_t timeout_ticks)
{
    uint32_t current_index = task_current_index();
    uint32_t saved_primask;

    if (arch_in_isr() != 0)
    {
        count_context_misuse(&g_sync_misuse_mutex_lock);
        return 0;
    }

    saved_primask = arch_critical_enter();

    if (mutex->locked == 0U)
    {
        mutex->locked = 1U;
        mutex->owner = current_index;
        mutex->recursion = 1U;
        arch_critical_exit(saved_primask);
        return 1;
    }
    if (mutex->owner == current_index)
    {
        mutex->recursion++;
        arch_critical_exit(saved_primask);
        return 1;
    }

    if (timeout_ticks == 0U)
    {
        arch_critical_exit(saved_primask);
        return 0;
    }

    task_inherit_priority(mutex->owner, task_current_priority());
    return task_block_locked(mutex, TASK_WAIT_MUTEX, timeout_ticks,
                             saved_primask);
}

int JRT_MutexUnlock(JRT_Mutex_t *mutex)
{
    if (arch_in_isr() != 0)
    {
        count_context_misuse(&g_sync_misuse_mutex_unlock);
        return 0;
    }

    uint32_t saved_primask = arch_critical_enter();
    uint32_t owner_id;
    uint32_t next_owner_id;

    if ((mutex->locked == 0U) || (mutex->owner != task_current_index()))
    {
        arch_critical_exit(saved_primask);
        return 0;
    }
    if (mutex->recursion > 1U)
    {
        mutex->recursion--;
        arch_critical_exit(saved_primask);
        return 1;
    }
    owner_id = mutex->owner;
    next_owner_id = task_wake_get_id(mutex, TASK_WAIT_MUTEX);
    if (next_owner_id != UINT32_MAX)
    {
        mutex->owner = next_owner_id;
        mutex->locked = 1U;
        mutex->recursion = 1U;
    }
    else
    {
        mutex->owner = UINT32_MAX;
        mutex->locked = 0U;
        mutex->recursion = 0U;
    }
    task_restore_priority(owner_id);
    if (next_owner_id != UINT32_MAX)
    {
        task_restore_priority(next_owner_id);
    }
    arch_critical_exit(saved_primask);
    return 1;
}

void JRT_QueueCreateStatic(JRT_Queue_t *queue, void *storage,
                           uint32_t capacity, uint32_t item_size)
{
    uint32_t saved_primask = arch_critical_enter();

    queue->storage = (uint8_t *)storage;
    queue->capacity = capacity;
    queue->item_size = item_size;
    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;
    queue->send_reservations = 0U;
    queue->receive_reservations = 0U;
    arch_critical_exit(saved_primask);
}

int JRT_QueueSend(JRT_Queue_t *queue, const void *item, uint32_t timeout_ticks)
{
    uint32_t reserved = 0U;

    if (arch_in_isr() != 0)
    {
        count_context_misuse(&g_sync_misuse_queue_send);
        return 0;
    }

    while (1)
    {
        uint32_t saved_primask = arch_critical_enter();

        if ((queue->capacity != 0U) && (queue->item_size != 0U)
            && ((reserved != 0U)
                || ((queue->count + queue->send_reservations)
                    < queue->capacity)))
        {
            uint8_t *destination = &queue->storage[queue->head * queue->item_size];
            const uint8_t *source = (const uint8_t *)item;
            uint32_t index;

            for (index = 0U; index < queue->item_size; index++)
            {
                destination[index] = source[index];
            }
            if (reserved != 0U)
            {
                queue->send_reservations--;
            }
            queue->head = (queue->head + 1U) % queue->capacity;
            queue->count++;
            if (task_wake(queue, TASK_WAIT_QUEUE_RECEIVE) != 0)
            {
                queue->receive_reservations++;
            }
            arch_critical_exit(saved_primask);
            return 1;
        }

        if (timeout_ticks == 0U)
        {
            arch_critical_exit(saved_primask);
            return 0;
        }
        if (task_block_locked(queue, TASK_WAIT_QUEUE_SEND, timeout_ticks,
                              saved_primask) == 0)
        {
            return 0;
        }
        reserved = 1U;
    }
}

int JRT_QueueReceive(JRT_Queue_t *queue, void *item, uint32_t timeout_ticks)
{
    uint32_t reserved = 0U;

    if (arch_in_isr() != 0)
    {
        count_context_misuse(&g_sync_misuse_queue_receive);
        return 0;
    }

    while (1)
    {
        uint32_t saved_primask = arch_critical_enter();

        if ((queue->item_size != 0U)
            && ((reserved != 0U)
                || (queue->count > queue->receive_reservations)))
        {
            const uint8_t *source = &queue->storage[queue->tail * queue->item_size];
            uint8_t *destination = (uint8_t *)item;
            uint32_t index;

            for (index = 0U; index < queue->item_size; index++)
            {
                destination[index] = source[index];
            }
            if (reserved != 0U)
            {
                queue->receive_reservations--;
            }
            queue->tail = (queue->tail + 1U) % queue->capacity;
            queue->count--;
            if (task_wake(queue, TASK_WAIT_QUEUE_SEND) != 0)
            {
                queue->send_reservations++;
            }
            arch_critical_exit(saved_primask);
            return 1;
        }

        if (timeout_ticks == 0U)
        {
            arch_critical_exit(saved_primask);
            return 0;
        }
        if (task_block_locked(queue, TASK_WAIT_QUEUE_RECEIVE, timeout_ticks,
                              saved_primask) == 0)
        {
            return 0;
        }
        reserved = 1U;
    }
}

int JRT_QueueSendFromISR(JRT_Queue_t *queue, const void *item)
{
    if (arch_in_isr() == 0)
    {
        count_context_misuse(&g_sync_misuse_queue_send_from_isr);
        return 0;
    }

    uint32_t saved_primask = arch_critical_enter();

    g_isr_queue_send_attempted++;

    if ((queue->capacity != 0U) && (queue->item_size != 0U)
        && ((queue->count + queue->send_reservations) < queue->capacity))
    {
        uint8_t *destination = &queue->storage[queue->head * queue->item_size];
        const uint8_t *source = (const uint8_t *)item;
        uint32_t index;

        for (index = 0U; index < queue->item_size; index++)
        {
            destination[index] = source[index];
        }
        queue->head = (queue->head + 1U) % queue->capacity;
        queue->count++;
        g_isr_queue_send_accepted++;
        if (queue->count > g_isr_queue_count_high_water)
        {
            g_isr_queue_count_high_water = queue->count;
        }
        if (task_wake(queue, TASK_WAIT_QUEUE_RECEIVE) != 0)
        {
            queue->receive_reservations++;
        }
        arch_critical_exit(saved_primask);
        arch_request_switch();
        return 1;
    }

    g_isr_queue_send_dropped++;
    arch_critical_exit(saved_primask);
    return 0;
}
