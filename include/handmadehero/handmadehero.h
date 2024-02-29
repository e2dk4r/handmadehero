#ifndef HANDMADEHERO_H

#include "types.h"

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

struct game_input {
  struct game_controller_input controllers[4];
};

void GameUpdateAndRender(struct game_input *input,
                         struct game_backbuffer *backbuffer);

#endif
