#ifndef JUSTRT_MEMPOOL_H
#define JUSTRT_MEMPOOL_H

#include <stdint.h>

typedef struct
{
    uint8_t *storage;
    uint32_t *used_bitmap;
    uint32_t block_size;
    uint32_t block_count;
} JRT_MemoryPool_t;

void JRT_MemoryPoolCreateStatic(JRT_MemoryPool_t *pool, void *storage,
                                uint32_t block_size, uint32_t block_count,
                                uint32_t *used_bitmap);
void *JRT_MemoryPoolAllocate(JRT_MemoryPool_t *pool);
int JRT_MemoryPoolFree(JRT_MemoryPool_t *pool, void *block);

#endif
