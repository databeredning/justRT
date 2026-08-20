#include "kernel.h"
#include "sync.h"

void semaphore_init(semaphore_t *semaphore, uint32_t initially_available)
{
    uint32_t saved_primask = critical_enter();

    semaphore->available = (initially_available != 0U) ? 1U : 0U;
    critical_exit(saved_primask);
}

int semaphore_take(semaphore_t *semaphore, uint32_t timeout_ticks)
{
    while (1)
    {
        uint32_t saved_primask = critical_enter();

        if (semaphore->available != 0U)
        {
            semaphore->available = 0U;
            critical_exit(saved_primask);
            return 1;
        }
        critical_exit(saved_primask);

        if (timeout_ticks == 0U)
        {
            return 0;
        }

        if (task_block(semaphore, TASK_WAIT_SEMAPHORE, timeout_ticks) == 0)
        {
            return 0;
        }
    }
}

void semaphore_give(semaphore_t *semaphore)
{
    uint32_t saved_primask = critical_enter();

    semaphore->available = 1U;
    task_wake(semaphore, TASK_WAIT_SEMAPHORE);
    critical_exit(saved_primask);
}

void semaphore_give_from_isr(semaphore_t *semaphore)
{
    uint32_t saved_primask = critical_enter();

    semaphore->available = 1U;
    task_wake(semaphore, TASK_WAIT_SEMAPHORE);
    critical_exit(saved_primask);
    request_switch();
}

void mutex_init(mutex_t *mutex)
{
    uint32_t saved_primask = critical_enter();

    mutex->locked = 0U;
    mutex->owner = UINT32_MAX;
    mutex->recursion = 0U;
    critical_exit(saved_primask);
}

int mutex_lock(mutex_t *mutex, uint32_t timeout_ticks)
{
    uint32_t current_index = task_current_index();

    while (1)
    {
        uint32_t saved_primask = critical_enter();

        if (mutex->locked == 0U)
        {
            mutex->locked = 1U;
            mutex->owner = current_index;
            mutex->recursion = 1U;
            critical_exit(saved_primask);
            return 1;
        }
        if (mutex->owner == current_index)
        {
            mutex->recursion++;
            critical_exit(saved_primask);
            return 1;
        }
        task_inherit_priority(mutex->owner, task_current_priority());
        critical_exit(saved_primask);

        if (timeout_ticks == 0U)
        {
            return 0;
        }
        if (task_block(mutex, TASK_WAIT_MUTEX, timeout_ticks) == 0)
        {
            return 0;
        }
    }
}

int mutex_unlock(mutex_t *mutex)
{
    uint32_t saved_primask = critical_enter();
    uint32_t owner_id;

    if ((mutex->locked == 0U) || (mutex->owner != task_current_index()))
    {
        critical_exit(saved_primask);
        return 0;
    }
    if (mutex->recursion > 1U)
    {
        mutex->recursion--;
        critical_exit(saved_primask);
        return 1;
    }
    owner_id = mutex->owner;
    mutex->owner = UINT32_MAX;
    mutex->locked = 0U;
    mutex->recursion = 0U;
    task_restore_priority(owner_id);
    task_wake(mutex, TASK_WAIT_MUTEX);
    critical_exit(saved_primask);
    return 1;
}

void queue_init(queue_t *queue, void *storage, uint32_t capacity, uint32_t item_size)
{
    uint32_t saved_primask = critical_enter();

    queue->storage = (uint8_t *)storage;
    queue->capacity = capacity;
    queue->item_size = item_size;
    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;
    critical_exit(saved_primask);
}

int queue_send(queue_t *queue, const void *item, uint32_t timeout_ticks)
{
    while (1)
    {
        uint32_t saved_primask = critical_enter();

        if ((queue->capacity != 0U) && (queue->item_size != 0U)
            && (queue->count < queue->capacity))
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
            task_wake(queue, TASK_WAIT_QUEUE_RECEIVE);
            critical_exit(saved_primask);
            return 1;
        }
        critical_exit(saved_primask);

        if (timeout_ticks == 0U)
        {
            return 0;
        }
        if (task_block(queue, TASK_WAIT_QUEUE_SEND, timeout_ticks) == 0)
        {
            return 0;
        }
    }
}

int queue_receive(queue_t *queue, void *item, uint32_t timeout_ticks)
{
    while (1)
    {
        uint32_t saved_primask = critical_enter();

        if ((queue->item_size != 0U) && (queue->count != 0U))
        {
            const uint8_t *source = &queue->storage[queue->tail * queue->item_size];
            uint8_t *destination = (uint8_t *)item;
            uint32_t index;

            for (index = 0U; index < queue->item_size; index++)
            {
                destination[index] = source[index];
            }
            queue->tail = (queue->tail + 1U) % queue->capacity;
            queue->count--;
            task_wake(queue, TASK_WAIT_QUEUE_SEND);
            critical_exit(saved_primask);
            return 1;
        }
        critical_exit(saved_primask);

        if (timeout_ticks == 0U)
        {
            return 0;
        }
        if (task_block(queue, TASK_WAIT_QUEUE_RECEIVE, timeout_ticks) == 0)
        {
            return 0;
        }
    }
}

int queue_send_from_isr(queue_t *queue, const void *item)
{
    uint32_t saved_primask = critical_enter();

    if ((queue->capacity != 0U) && (queue->item_size != 0U)
        && (queue->count < queue->capacity))
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
        task_wake(queue, TASK_WAIT_QUEUE_RECEIVE);
        critical_exit(saved_primask);
        request_switch();
        return 1;
    }

    critical_exit(saved_primask);
    return 0;
}
