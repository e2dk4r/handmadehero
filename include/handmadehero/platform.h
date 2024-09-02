#ifndef HANDMADEHERO_PLATFORM_H
#define HANDMADEHERO_PLATFORM_H

#include "assert.h"
#include "errors.h"
#include "types.h"

struct platform_file_handle {
  enum handmadehero_error error;
  void *data;
};

struct platform_file_group {
  u32 fileCount;
  void *data;
};

enum platform_file_type {
  PLATFORM_FILE_TYPE_ASSET_FILE,
  PLATFORM_FILE_TYPE_SAVE_FILE,

  PLATFORM_FILE_TYPE_COUNT,
};

typedef struct platform_file_handle (*pfnPlatformOpenNextFile)(struct platform_file_group *fileGroup);
typedef void (*pfnPlatformReadFromFile)(void *dest, struct platform_file_handle *handle, u64 offset, u64 size);
typedef struct platform_file_group (*pfnPlatformGetAllFilesOfTypeBegin)(enum platform_file_type type);
typedef void (*pfnPlatformGetAllFilesOfTypeEnd)(struct platform_file_group *fileGroup);
typedef void (*pfnPlatformFileError)(struct platform_file_handle *handle, enum handmadehero_error error);
typedef b32 (*pfnPlatformHasFileError)(struct platform_file_handle *handle);
typedef void *(*pfnPlatformAllocateMemory)(memory_arena_size_t size);
typedef void (*pfnPlatformDeallocateMemory)(void *memory);

#if HANDMADEHERO_INTERNAL

struct read_file_result {
  u64 size;
  void *data;
};

struct read_file_result
PlatformReadEntireFile(char *path);
typedef struct read_file_result (*pfnPlatformReadEntireFile)(char *path);

u8
PlatformWriteEntireFile(char *path, u64 size, void *data);
typedef u8 (*pfnPlatformWriteEntireFile)(char *path, u64 size, void *data);

void
PlatformFreeMemory(void *address);
typedef void (*pfnPlatformFreeMemory)(void *address);

static __inline__ u64
rdtsc(void)
{
  u64 hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | hi << 32;
}

extern struct game_memory *DEBUG_GLOBAL_MEMORY;

#endif /* HANDMADEHERO_INTERNAL */

struct game_backbuffer {
  u32 width;
  u32 height;
  u32 stride;
  void *memory;
};

struct game_button_state {
  u32 halfTransitionCount;
  u8 pressed : 1;
};

struct game_controller_input {
  u8 isAnalog : 1;

  /* normalized where values are [0, 1] */
  f32 stickAverageX;
  /* normalized where values are [0, 1] */
  f32 stickAverageY;

  union {
    struct game_button_state buttons[12];
    struct {
      struct game_button_state moveDown;
      struct game_button_state moveUp;
      struct game_button_state moveLeft;
      struct game_button_state moveRight;

      struct game_button_state actionUp;
      struct game_button_state actionDown;
      struct game_button_state actionLeft;
      struct game_button_state actionRight;

      struct game_button_state leftShoulder;
      struct game_button_state rightShoulder;

      struct game_button_state start;
      struct game_button_state back;
    };
  };
};

struct game_input {
  u32 pointerX;
  u32 pointerY;

  /*
   *        |-----|-----|-----|-----|-----|--->
   *  frame 0     1     2     3     4     5
   *        ⇐ ∆t  ⇒
   *  in seconds
   */
  f32 dtPerFrame;
  struct game_controller_input controllers[5];

#if HANDMADEHERO_DEBUG
  u8 gameCodeReloaded : 1;
#endif
};

internal inline struct game_controller_input *
GetController(struct game_input *input, u8 index)
{
  assert(index < ARRAY_COUNT(input->controllers));
  return &input->controllers[index];
}

struct platform_work_queue;
typedef void (*pfnPlatformWorkQueueCallback)(struct platform_work_queue *queue, void *data);
typedef void (*pfnPlatformWorkQueueAddEntry)(struct platform_work_queue *queue, pfnPlatformWorkQueueCallback callback,
                                             void *data);
typedef void (*pfnPlatformWorkQueueCompleteAllWork)(struct platform_work_queue *queue);

struct platform_work_queue_entry {
  void *data;
  pfnPlatformWorkQueueCallback callback;
};

struct platform_api {
  pfnPlatformWorkQueueAddEntry WorkQueueAddEntry;
  pfnPlatformWorkQueueCompleteAllWork WorkQueueCompleteAllWork;

  pfnPlatformOpenNextFile OpenNextFile;
  pfnPlatformReadFromFile ReadFromFile;
  pfnPlatformHasFileError HasFileError;
  pfnPlatformFileError FileError;
  pfnPlatformGetAllFilesOfTypeBegin GetAllFilesOfTypeBegin;
  pfnPlatformGetAllFilesOfTypeEnd GetAllFilesOfTypeEnd;

  pfnPlatformAllocateMemory AllocateMemory;
  pfnPlatformDeallocateMemory DeallocateMemory;

#if HANDMADEHERO_DEBUG
  pfnPlatformReadEntireFile ReadEntireFile;
  pfnPlatformWriteEntireFile WriteEntireFile;
  pfnPlatformFreeMemory FreeMemory;
#endif
};

struct game_memory {
  u64 permanentStorageSize;
  void *permanentStorage;

  u64 transientStorageSize;
  void *transientStorage;

  u64 debugStorageSize;
  void *debugStorage;

  struct platform_work_queue *highPriorityQueue;
  struct platform_work_queue *lowPriorityQueue;

  struct platform_api platform;

#if HANDMADEHERO_INTERNAL
  struct render_group *DEBUGtextRenderGroup;
#endif
};

extern struct platform_api *Platform;

void
GameUpdateAndRender(struct game_memory *memory, struct game_input *input, struct game_backbuffer *backbuffer);
typedef void (*pfnGameUpdateAndRender)(struct game_memory *memory, struct game_input *input,
                                       struct game_backbuffer *backbuffer);

struct game_audio_buffer {
  u32 sampleRate;
  u32 sampleCount;
  s16 *samples;
};

b32
GameOutputAudio(struct game_memory *memory, struct game_audio_buffer *buffer);
typedef b32 (*pfnGameOutputAudio)(struct game_memory *memory, struct game_audio_buffer *buffer);

struct game_frame_info {
};

void
GameFrameEnd(struct game_memory *memory, struct game_frame_info *info);
typedef void (*pfnGameFrameEnd)(struct game_memory *memory, struct game_frame_info *info);

/*****************************************************************
 * DEBUG TIMERS
 *****************************************************************/

#if HANDMADEHERO_INTERNAL
#include "atomic.h"

struct timed_block {
  u64 startCycles;
  // hitCount stored in high 32 bits, and cycleCount in low
  u64 hitCount_cycleCount;

  char *filename;
  char *function;

  u32 hitCount;
  u32 line;
};

extern struct timed_block TIMED_BLOCKS[];

internal inline struct timed_block *
BeginTimedBlock(u32 timedBlockIndex, u32 count, char *filename, u32 line, char *function)
{
  struct timed_block *timedBlock = TIMED_BLOCKS + timedBlockIndex;

  timedBlock->filename = filename;
  timedBlock->function = function;
  timedBlock->line = line;
  timedBlock->startCycles = rdtsc();
  timedBlock->hitCount = count;

  return timedBlock;
}

internal inline void
EndTimedBlock(struct timed_block **timedBlockPtr)
{
  struct timed_block *timedBlock = *timedBlockPtr;

  u64 elapsedCycles = rdtsc() - timedBlock->startCycles;
  u64 shiftedHitCount = (u64)timedBlock->hitCount << 32;
  AtomicFetchAdd(&timedBlock->hitCount_cycleCount, elapsedCycles | shiftedHitCount);
}

// TODO: MSVC?
#define TIMED_BLOCK() __attribute__((cleanup(EndTimedBlock))) BEGIN_TIMED_BLOCK(__LINE__)
#define TIMED_BLOCK_COUNTED(count)                                                                                     \
  __attribute((cleanup(EndTimedBlock))) struct timed_block *timedBlock##tag =                                          \
      BeginTimedBlock(__COUNTER__, count, __FILE__, __LINE__, (char *)__FUNCTION__)

#define BEGIN_TIMED_BLOCK(tag)                                                                                         \
  struct timed_block *timedBlock##tag = BeginTimedBlock(__COUNTER__, 1, __FILE__, __LINE__, (char *)__FUNCTION__)
#define END_TIMED_BLOCK(tag) EndTimedBlock(&timedBlock##tag)

#else

#define BEGIN_TIMED_BLOCK(tag)
#define END_TIMED_BLOCK(tag)

#endif

struct debug_counter_snapshot {
  u32 hitCount;
  u32 cycleCount;
};

struct debug_counter_state {
  char *filename;
  char *function;
  u32 line;

  struct debug_counter_snapshot snapshots[128];
};

struct debug_state {
  struct debug_counter_state counterStates[512];
  u32 counterCount;
};

#endif /* HANDMADEHERO_PLATFORM_H */
