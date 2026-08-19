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
