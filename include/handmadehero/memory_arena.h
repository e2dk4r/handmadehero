#ifndef HANDMADEHERO_MEMORY_ARENA_H
#define HANDMADEHERO_MEMORY_ARENA_H

#include "types.h"

typedef u64 memory_arena_size_t;
struct memory_arena {
  u64 used;
  memory_arena_size_t size;
  void *data;
};

struct memory_chunk {
  void *block;
  u64 size;
  u64 max;
};

void
MemoryArenaInit(struct memory_arena *mem, void *data, memory_arena_size_t size);
void *
MemoryArenaPush(struct memory_arena *mem, memory_arena_size_t size);

void *
MemoryChunkPush(struct memory_chunk *chunk);
void
MemoryChunkPop(struct memory_chunk *chunk, void *block);
struct memory_chunk *
MemoryArenaPushChunk(struct memory_arena *mem, u64 size, u64 max);

void
ZeroMemory(void *ptr, memory_arena_size_t size);
#define ZeroStruct(ptr) ZeroMemory(ptr, sizeof(ptr))

#endif /* HANDMADEHERO_MEMORY_ARENA_H */
