#ifndef HANDMADEHERO_PLATFORM_H
#define HANDMADEHERO_PLATFORM_H

#include "assert.h"
#include "types.h"

#if HANDMADEHERO_INTERNAL

struct read_file_result {
  u64 size;
  void *data;
};
struct read_file_result PlatformReadEntireFile(char *path);
u8 PlatformWriteEntireFile(char *path, u64 size, void *data);
void PlatformFreeMemory(void *address);

#endif

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

  f32 stickAverageX;
  f32 stickAverageY;

  union {
#define GAME_CONTROLLER_BUTTON_COUNT 12
    struct game_button_state buttons[GAME_CONTROLLER_BUTTON_COUNT];
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

#define HANDMADEHERO_CONTROLLER_COUNT 2U
struct game_input {
  f32 dtPerFrame;
  struct game_controller_input controllers[HANDMADEHERO_CONTROLLER_COUNT];
};

static inline struct game_controller_input *
GetController(struct game_input *input, u8 index) {
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


#endif /* HANDMADEHERO_PLATFORM_H */
