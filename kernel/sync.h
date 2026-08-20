#ifndef JUSTBOOT_SYNC_H
#define JUSTBOOT_SYNC_H

#include <stdint.h>

typedef struct
{
    volatile uint32_t available;
} semaphore_t;

typedef struct
{
    volatile uint32_t locked;
    volatile uint32_t owner;
    volatile uint32_t recursion;
} mutex_t;

#define SEMAPHORE_WAIT_FOREVER UINT32_MAX

typedef struct
{
    uint8_t *storage;
    uint32_t capacity;
    uint32_t item_size;
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
} queue_t;

void semaphore_init(semaphore_t *semaphore, uint32_t initially_available);
int semaphore_take(semaphore_t *semaphore, uint32_t timeout_ticks);
void semaphore_give(semaphore_t *semaphore);
void semaphore_give_from_isr(semaphore_t *semaphore);

void mutex_init(mutex_t *mutex);
int mutex_lock(mutex_t *mutex, uint32_t timeout_ticks);
int mutex_unlock(mutex_t *mutex);

void queue_init(queue_t *queue, void *storage, uint32_t capacity, uint32_t item_size);
int queue_send(queue_t *queue, const void *item, uint32_t timeout_ticks);
int queue_receive(queue_t *queue, void *item, uint32_t timeout_ticks);
int queue_send_from_isr(queue_t *queue, const void *item);

#endif
