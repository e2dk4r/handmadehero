#ifndef HANDMADEHERO_H

#include "types.h"
#include "assert.h"

/****************************************************************
 * Platform Layer
 ****************************************************************/
#if HANDMADEHERO_INTERNAL

struct read_file_result {
  u64 size;
  void *data;
};
struct read_file_result PlatformReadEntireFile(char *path);
u8 PlatformWriteEntireFile(char *path, u64 size, void *data);
void PlatformFreeMemory(void *address);

#endif

/****************************************************************
 * Game Layer
 ****************************************************************/
struct game_backbuffer {
  u32 width;
  u32 height;
  /* stride = width * bytes_per_pixel */
  u32 stride;
  /* how much bytes required for one pixel */
  u32 bytes_per_pixel;
  void *memory;
};

struct game_button_state {
  u32 halfTransitionCount;
  u8 pressed : 1;
};

struct game_controller_input {
  u8 isAnalog : 1;

  f32 startX;
  f32 startY;

  f32 minX;
  f32 minY;

  f32 maxX;
  f32 maxY;

  f32 endX;
  f32 endY;

  union {
    struct game_button_state buttons[6];
    struct {
      struct game_button_state down;
      struct game_button_state up;
      struct game_button_state left;
      struct game_button_state right;
      struct game_button_state leftShoulder;
      struct game_button_state rightShoulder;
    };
  };
};

#define HANDMADEHERO_CONTROLLER_COUNT 2U
struct game_input {
  struct game_controller_input controllers[HANDMADEHERO_CONTROLLER_COUNT];
};

static inline struct game_controller_input * GetController(struct game_input *input, u8 index) {
  assert(index < sizeof(input->controllers) / sizeof(*input->controllers));
  return &input->controllers[index];
}

struct game_memory {
  u8 initialized : 1;

  u64 permanentStorageSize;
  void *permanentStorage;

  u64 transientStorageSize;
  void *transientStorage;
};

struct game_state {
  i32 greenOffset;
  i32 blueOffset;
};

#define GAMEUPDATEANDRENDER(name)                                              \
  void name(struct game_memory *memory, struct game_input *input,              \
            struct game_backbuffer *backbuffer)
GAMEUPDATEANDRENDER(GameUpdateAndRender);
typedef GAMEUPDATEANDRENDER(pfnGameUpdateAndRender);

#endif
