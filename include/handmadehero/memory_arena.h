#ifndef HANDMADEHERO_MEMORY_ARENA_H
#define HANDMADEHERO_MEMORY_ARENA_H

#include "types.h"

typedef u64 memory_arena_size_t;
struct memory_arena {
  memory_arena_size_t used;
  memory_arena_size_t size;
  void *data;

#if HANDMADEHERO_DEBUG
  i32 tempCount;
#endif
};

struct memory_chunk {
  void *block;
  memory_arena_size_t size;
  memory_arena_size_t max;
};

struct memory_temp {
  struct memory_arena *arena;
  u64 used;
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
MemoryArenaPushChunk(struct memory_arena *mem, memory_arena_size_t size, memory_arena_size_t max);

void
ZeroMemory(void *ptr, memory_arena_size_t size);

struct memory_temp
BeginTemporaryMemory(struct memory_arena *arena);

void
EndTemporaryMemory(struct memory_temp *temp);

void
MemoryArenaCheck(struct memory_arena *arena);

#endif /* HANDMADEHERO_MEMORY_ARENA_H */
