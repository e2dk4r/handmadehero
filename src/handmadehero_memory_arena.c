#include <handmadehero/assert.h>
#include <handmadehero/memory_arena.h>

inline void
MemoryArenaInit(struct memory_arena *mem, void *data, memory_arena_size_t size)
{
  mem->used = 0;
  mem->size = size;
  mem->data = data;
}

inline void *
MemoryArenaPush(struct memory_arena *mem, memory_arena_size_t size)
{
  assert(mem->used + size <= mem->size && "arena capacity exceeded");

  void *data = mem->data + mem->used;
  mem->used += size;

  return data;
}

inline void *
MemoryChunkPush(struct memory_chunk *chunk)
{
  void *result = 0;
  void *dataBlock = chunk->block + sizeof(u8) * chunk->max;
  for (u64 index = 0; index < chunk->max; index++) {
    u8 *flag = chunk->block + sizeof(u8) * index;
    if (*flag == 0) {
      result = dataBlock + index * chunk->size;
      *flag = 1;
      return result;
    }
  }

  return result;
}

inline void
MemoryChunkPop(struct memory_chunk *chunk, void *ptr)
{
  void *dataBlock = chunk->block + sizeof(u8) * chunk->max;
  assert(ptr >= dataBlock);
  u64 index = (u64)(ptr - dataBlock) / chunk->size;
  u8 *flag = chunk->block + sizeof(u8) * index;
  *flag = 0;
}

inline struct memory_chunk *
MemoryArenaPushChunk(struct memory_arena *mem, u64 size, u64 max)
{
  assert(size > 0);
  assert(max > 0);
  struct memory_chunk *chunk = MemoryArenaPush(mem, sizeof(*chunk) + max * sizeof(u8) + max * size);
  chunk->block = chunk + sizeof(*chunk);
  chunk->size = size;
  chunk->max = max;
  for (u64 index = 0; index < chunk->max; index++) {
    u8 *flag = chunk->block + sizeof(u8) * index;
    *flag = 0;
  }
  return chunk;
}

inline void
ZeroMemory(void *ptr, memory_arena_size_t size)
{
  __builtin_bzero(ptr, size);
}
