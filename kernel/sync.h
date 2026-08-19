#ifndef JUSTBOOT_SYNC_H
#define JUSTBOOT_SYNC_H

#include <stdint.h>

typedef struct
{
    volatile uint32_t available;
} semaphore_t;

#define SEMAPHORE_WAIT_FOREVER UINT32_MAX

void semaphore_init(semaphore_t *semaphore, uint32_t initially_available);
int semaphore_take(semaphore_t *semaphore, uint32_t timeout_ticks);
void semaphore_give(semaphore_t *semaphore);

#endif
