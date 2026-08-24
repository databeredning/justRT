#ifndef JUSTRT_SYNC_H
#define JUSTRT_SYNC_H

#include <stdint.h>

#define SYNC_PRIVILEGED __attribute__((section(".privileged_functions")))

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

void semaphore_init(semaphore_t *semaphore, uint32_t initially_available) SYNC_PRIVILEGED;
int semaphore_take(semaphore_t *semaphore, uint32_t timeout_ticks) SYNC_PRIVILEGED;
void semaphore_give(semaphore_t *semaphore) SYNC_PRIVILEGED;
/* Call from ISR only; wakes highest-priority waiter and pends PendSV. */
void semaphore_give_from_isr(semaphore_t *semaphore) SYNC_PRIVILEGED;

void mutex_init(mutex_t *mutex) SYNC_PRIVILEGED;
int mutex_lock(mutex_t *mutex, uint32_t timeout_ticks) SYNC_PRIVILEGED;
int mutex_unlock(mutex_t *mutex) SYNC_PRIVILEGED;

void queue_init(queue_t *queue, void *storage, uint32_t capacity, uint32_t item_size) SYNC_PRIVILEGED;
int queue_send(queue_t *queue, const void *item, uint32_t timeout_ticks) SYNC_PRIVILEGED;
int queue_receive(queue_t *queue, void *item, uint32_t timeout_ticks) SYNC_PRIVILEGED;
/* Call from ISR only; never blocks; returns 0 if queue is full. */
int queue_send_from_isr(queue_t *queue, const void *item) SYNC_PRIVILEGED;

#endif
