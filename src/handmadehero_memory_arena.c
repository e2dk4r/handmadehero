#include <handmadehero/assert.h>
#include <handmadehero/math.h>
#include <handmadehero/memory_arena.h>

inline void
MemoryArenaInit(struct memory_arena *mem, void *data, memory_arena_size_t size)
{
  mem->used = 0;
  mem->size = size;
  mem->data = data;
}

internal inline memory_arena_size_t
GetAlignmentOffset(struct memory_arena *mem, memory_arena_size_t alignment)
{
  assert(IsPowerOfTwo((s32)alignment) && "alignment must be power of 2");
  memory_arena_size_t alignmentMask = alignment - 1;
  memory_arena_size_t dataPointer = ((memory_arena_size_t)mem->data + mem->used);
  memory_arena_size_t alignmentOffset = 0;
  if (dataPointer & alignmentMask) {
    alignmentOffset = alignment - (dataPointer & alignmentMask);
  }
  return alignmentOffset;
}

inline void *
MemoryArenaPush(struct memory_arena *mem, memory_arena_size_t size)
{
  return MemoryArenaPushAlignment(mem, size, 4);
}

inline void *
MemoryArenaPushAlignment(struct memory_arena *mem, memory_arena_size_t size, memory_arena_size_t alignment)
{
  memory_arena_size_t alignmentOffset = GetAlignmentOffset(mem, alignment);
  size += alignmentOffset;

  assert(mem->used + size <= mem->size && "arena capacity exceeded");
  void *data = mem->data + mem->used + alignmentOffset;
  mem->used += size;

  assert(((memory_arena_size_t)data & (alignment - 1)) == 0 && "something went very wrong. cannot get memory aligned");

  return data;
}

inline memory_arena_size_t
MemoryArenaGetRemainingSize(struct memory_arena *mem)
{
  return MemoryArenaGetRemainingSizeAlignment(mem, 4);
}

inline memory_arena_size_t
MemoryArenaGetRemainingSizeAlignment(struct memory_arena *mem, memory_arena_size_t alignment)
{
  memory_arena_size_t remainingSize = mem->size - (mem->used + GetAlignmentOffset(mem, alignment));

  return remainingSize;
}

inline void
MemorySubArenaInit(struct memory_arena *sub, struct memory_arena *masterArena, memory_arena_size_t size)
{
  MemorySubArenaInitAlignment(sub, masterArena, size, 4);
}

inline void
MemorySubArenaInitAlignment(struct memory_arena *sub, struct memory_arena *masterArena, memory_arena_size_t size,
                            memory_arena_size_t alignment)
{
  sub->used = 0;
  sub->size = size;
  sub->data = MemoryArenaPushAlignment(masterArena, size, alignment);
#if HANDMADEHERO_DEBUG
  sub->tempCount = 0;
#endif
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
MemoryArenaPushChunk(struct memory_arena *mem, memory_arena_size_t size, memory_arena_size_t max)
{
  assert(size > 0);
  assert(max > 0);
  struct memory_chunk *chunk = MemoryArenaPush(mem, sizeof(*chunk) + max * sizeof(u8) + max * size);
  chunk->block = chunk + sizeof(*chunk);
  chunk->size = size;
  chunk->max = max;
  for (memory_arena_size_t index = 0; index < chunk->max; index++) {
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

inline struct memory_temp
BeginTemporaryMemory(struct memory_arena *arena)
{
  struct memory_temp temp;

  temp.arena = arena;
  temp.used = arena->used;

#if HANDMADEHERO_DEBUG
  arena->tempCount++;
#endif

  return temp;
}

inline void
EndTemporaryMemory(struct memory_temp *temp)
{
  struct memory_arena *arena = temp->arena;

  assert(arena->used >= temp->used);
  arena->used = temp->used;

#if HANDMADEHERO_DEBUG
  assert(arena->tempCount > 0);
  arena->tempCount--;
#endif
}

inline void
MemoryArenaCheck(struct memory_arena *arena)
{
  assert(arena->tempCount == 0);
}

internal inline u64
strlen(const char *str)
{
  u64 length = 0;
  while (*str++)
    length++;
  return length;
}

internal void *
memcpy(void *dest, const void *src, u64 size)
{
  while (size--)
    *(u8 *)dest++ = *(u8 *)src++;
  return dest;
}

inline char *
MemoryArenaPushString(struct memory_arena *mem, char *string)
{
  u64 length = strlen(string) + 1;
  u64 size = ALIGN(length, 4);
  char *dest = MemoryArenaPush(mem, size);
  memcpy(dest, string, length);
  dest[length] = 0;
  return dest;
}
