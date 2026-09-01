#ifndef JUSTRT_SYNC_H
#define JUSTRT_SYNC_H

#include <stdint.h>

#define SYNC_PRIVILEGED __attribute__((section(".privileged_functions")))

typedef struct
{
    volatile uint32_t available;
} JRT_Semaphore_t;

typedef struct JRT_Mutex
{
    volatile uint32_t locked;
    volatile uint32_t owner;
    volatile uint32_t recursion;
    struct JRT_Mutex *next;
} JRT_Mutex_t;

#define JRT_WAIT_FOREVER UINT32_MAX

typedef struct
{
    uint8_t *storage;
    uint32_t capacity;
    uint32_t item_size;
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint32_t count;
    volatile uint32_t send_reservations;
    volatile uint32_t receive_reservations;
} JRT_Queue_t;

void JRT_SemaphoreCreateBinaryStatic(JRT_Semaphore_t *semaphore, uint32_t initially_available) SYNC_PRIVILEGED;
int JRT_SemaphoreTake(JRT_Semaphore_t *semaphore, uint32_t timeout_ticks) SYNC_PRIVILEGED;
void JRT_SemaphoreGive(JRT_Semaphore_t *semaphore) SYNC_PRIVILEGED;
/* Call from ISR only; wakes highest-priority waiter and pends PendSV. */
void JRT_SemaphoreGiveFromISR(JRT_Semaphore_t *semaphore) SYNC_PRIVILEGED;

void JRT_MutexCreateRecursiveStatic(JRT_Mutex_t *mutex) SYNC_PRIVILEGED;
int JRT_MutexLock(JRT_Mutex_t *mutex, uint32_t timeout_ticks) SYNC_PRIVILEGED;
int JRT_MutexUnlock(JRT_Mutex_t *mutex) SYNC_PRIVILEGED;

void JRT_QueueCreateStatic(JRT_Queue_t *queue, void *storage, uint32_t capacity, uint32_t item_size) SYNC_PRIVILEGED;
int JRT_QueueSend(JRT_Queue_t *queue, const void *item, uint32_t timeout_ticks) SYNC_PRIVILEGED;
int JRT_QueueReceive(JRT_Queue_t *queue, void *item, uint32_t timeout_ticks) SYNC_PRIVILEGED;
/* Call from ISR only; never blocks; returns 0 if queue is full. */
int JRT_QueueSendFromISR(JRT_Queue_t *queue, const void *item) SYNC_PRIVILEGED;

/* Kernel-internal invariant validation. Caller holds a critical section. */
uint32_t sync_invariant_check(uint32_t task_count, uintptr_t *object) SYNC_PRIVILEGED;

#endif
