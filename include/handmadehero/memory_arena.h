#ifndef HANDMADEHERO_MEMORY_ARENA_H
#define HANDMADEHERO_MEMORY_ARENA_H

#include "types.h"

typedef u64 memory_arena_size_t;
struct memory_arena {
  u64 used;
  memory_arena_size_t size;
  void *data;
};

void MemoryArenaInit(struct memory_arena *mem, void *data, memory_arena_size_t size);
void *MemoryArenaPush(struct memory_arena *mem, memory_arena_size_t size);

#endif /* HANDMADEHERO_MEMORY_ARENA_H */
