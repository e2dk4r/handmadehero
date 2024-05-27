#ifndef HANDMADEHERO_PLATFORM_H
#define HANDMADEHERO_PLATFORM_H

#include "assert.h"
#include "types.h"

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

enum {
  CYCLE_COUNTER_GameUpdateAndRender,
  CYCLE_COUNTER_DrawRenderGroup,
  CYCLE_COUNTER_DrawRectangleSlowly,
  CYCLE_COUNTER_ProcessPixel,
  CYCLE_COUNTER_DrawRectangleQuickly,
  CYCLE_COUNTER_COUNT
};

struct cycle_counter {
  u64 cycleCount;
  u64 hitCount;
};

u64
rdtsc(void);
extern struct game_memory *DEBUG_GLOBAL_MEMORY;
#define BEGIN_TIMER_BLOCK(tag) u64 startCycleCount##tag = rdtsc()
#define END_TIMER_BLOCK(tag)                                                                                           \
  DEBUG_GLOBAL_MEMORY->counters[CYCLE_COUNTER_##tag].cycleCount += rdtsc() - startCycleCount##tag;                     \
  DEBUG_GLOBAL_MEMORY->counters[CYCLE_COUNTER_##tag].hitCount += 1;
#define END_TIMER_BLOCK_COUNTED(tag, count)                                                                            \
  DEBUG_GLOBAL_MEMORY->counters[CYCLE_COUNTER_##tag].cycleCount += rdtsc() - startCycleCount##tag;                     \
  DEBUG_GLOBAL_MEMORY->counters[CYCLE_COUNTER_##tag].hitCount += (count);

#else

#define BEGIN_TIMER_BLOCK(tag)
#define END_TIMER_BLOCK(tag)
#define END_TIMER_BLOCK_COUNTED(tag, count)

#endif

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

struct game_memory {
  u8 initialized : 1;

  u64 permanentStorageSize;
  void *permanentStorage;

  u64 transientStorageSize;
  void *transientStorage;

#if HANDMADEHERO_INTERNAL
  pfnPlatformReadEntireFile PlatformReadEntireFile;
  pfnPlatformWriteEntireFile PlatformWriteEntireFile;
  pfnPlatformFreeMemory PlatformFreeMemory;

  struct cycle_counter counters[CYCLE_COUNTER_COUNT];
#endif
};

#endif /* HANDMADEHERO_PLATFORM_H */
