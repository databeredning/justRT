#include "kernel.h"
#include "mempool.h"
#include "cortex_m/port_contract.h"

static uint32_t bitmap_words(uint32_t block_count)
{
    return (block_count + 31U) / 32U;
}

void JRT_MemoryPoolCreateStatic(JRT_MemoryPool_t *pool, void *storage,
                                uint32_t block_size, uint32_t block_count,
                                uint32_t *used_bitmap)
{
    uint32_t index;

    if (pool == 0U || storage == 0U || used_bitmap == 0U
        || block_size < sizeof(uintptr_t) || block_count == 0U)
    {
        return;
    }

    pool->storage = (uint8_t *)storage;
    pool->used_bitmap = used_bitmap;
    pool->block_size = block_size;
    pool->block_count = block_count;
    for (index = 0U; index < bitmap_words(block_count); index++)
    {
        used_bitmap[index] = 0U;
    }
}

void *JRT_MemoryPoolAllocate(JRT_MemoryPool_t *pool)
{
    uint32_t saved_primask;
    uint32_t index;

    if (pool == 0U || pool->storage == 0U || pool->used_bitmap == 0U)
    {
        return 0U;
    }

    saved_primask = arch_critical_enter();
    for (index = 0U; index < pool->block_count; index++)
    {
        uint32_t word = index / 32U;
        uint32_t mask = 1UL << (index % 32U);

        if ((pool->used_bitmap[word] & mask) == 0U)
        {
            pool->used_bitmap[word] |= mask;
            arch_critical_exit(saved_primask);
            return &pool->storage[index * pool->block_size];
        }
    }
    arch_critical_exit(saved_primask);
    return 0U;
}

int JRT_MemoryPoolFree(JRT_MemoryPool_t *pool, void *block)
{
    uintptr_t address;
    uintptr_t start;
    uintptr_t offset;
    uint32_t index;
    uint32_t word;
    uint32_t mask;
    uint32_t saved_primask;

    if (pool == 0U || block == 0U || pool->storage == 0U
        || pool->used_bitmap == 0U || pool->block_size == 0U)
    {
        return 0;
    }

    address = (uintptr_t)block;
    start = (uintptr_t)pool->storage;
    if (address < start
        || address >= (start + (pool->block_size * pool->block_count)))
    {
        return 0;
    }

    offset = address - start;
    if ((offset % pool->block_size) != 0U)
    {
        return 0;
    }

    index = (uint32_t)(offset / pool->block_size);
    word = index / 32U;
    mask = 1UL << (index % 32U);
    saved_primask = arch_critical_enter();
    if ((pool->used_bitmap[word] & mask) == 0U)
    {
        arch_critical_exit(saved_primask);
        return 0;
    }
    pool->used_bitmap[word] &= ~mask;
    arch_critical_exit(saved_primask);
    return 1;
}
