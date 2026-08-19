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

        sleep_ticks(1U);
        if (timeout_ticks != SEMAPHORE_WAIT_FOREVER)
        {
            timeout_ticks--;
        }
    }
}

void semaphore_give(semaphore_t *semaphore)
{
    uint32_t saved_primask = critical_enter();

    semaphore->available = 1U;
    critical_exit(saved_primask);
}
