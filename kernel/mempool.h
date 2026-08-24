#ifndef JUSTRT_MEMPOOL_H
#define JUSTRT_MEMPOOL_H

#include <stdint.h>

typedef struct
{
    uint8_t *storage;
    uint32_t *used_bitmap;
    uint32_t block_size;
    uint32_t block_count;
} memory_pool_t;

void memory_pool_init(memory_pool_t *pool, void *storage, uint32_t block_size,
                      uint32_t block_count, uint32_t *used_bitmap);
void *memory_pool_alloc(memory_pool_t *pool);
int memory_pool_free(memory_pool_t *pool, void *block);

#endif
