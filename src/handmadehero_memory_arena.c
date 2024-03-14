#include <handmadehero/assert.h>
#include <handmadehero/memory_arena.h>

void MemoryArenaInit(struct memory_arena *mem, void *data, memory_arena_size_t size) {
  mem->used = 0;
  mem->size = size;
  mem->data = data;
}

void *MemoryArenaPush(struct memory_arena *mem, memory_arena_size_t size) {
  assert(mem->used + size <= mem->size && "arena capacity exceeded");

  void *data = mem->data + mem->used;
  mem->used += size;

  return data;
}

