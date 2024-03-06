#include <handmadehero/assert.h>
#include <handmadehero/handmadehero.h>

static void draw_frame(struct game_backbuffer *backbuffer, int blueOffset,
                       int greenOffset) {
  u8 *row = backbuffer->memory;
  for (u32 y = 0; y < backbuffer->height; y++) {
    u32 *pixel = (u32 *)row;
    for (u32 x = 0; x < backbuffer->width; x++) {
      u8 blue = (u8)((int)x + blueOffset);
      u8 green = (u8)((int)y + greenOffset);

      *pixel++ = (u32)((green << 8) | blue);
    }
    row += backbuffer->stride;
  }
}

static void draw_player(struct game_backbuffer *backbuffer, i32 playerX,
                        i32 playerY) {
  u32 height = 10;
  u32 top = (u32)playerY;
  u32 bottom = top + height;

  u32 width = 10;
  u32 left = (u32)playerX;
  u32 right = left + width;

  u32 color = 0xffffff;

  for (u32 x = left; x < right; x++) {
    u8 *pixel = (backbuffer->memory
                 /* x offset */
                 + (x * backbuffer->bytes_per_pixel)
                 /* y offset */
                 + (top * backbuffer->stride));
    for (u32 y = top; y < bottom; y++) {
      *(u32 *)pixel = color;
      pixel += backbuffer->stride;
    }
  }
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct game_state *state = memory->permanentStorage;
  if (!memory->initialized) {
    state->blueOffset = 0;
    state->greenOffset = 0;
    state->playerX = 100;
    state->playerY = 100;

    memory->initialized = 1;
  }

  for (u8 controllerIndex = 0; controllerIndex < 2; controllerIndex++) {
    struct game_controller_input *controller =
        GetController(input, controllerIndex);
    if (controller->isAnalog) {
      state->blueOffset += (int)(4.0f * controller->stickAverageX);
      state->greenOffset += (int)(4.0f * controller->stickAverageY);
    }

    if (controller->moveLeft.pressed) {
      state->blueOffset -= 1;
    }

    if (controller->moveRight.pressed) {
      state->blueOffset += 1;
    }

    if (controller->moveDown.pressed) {
      state->greenOffset += 1;
    }

    if (controller->moveUp.pressed) {
      state->greenOffset -= 1;
    }

    if (controller->actionUp.pressed) {
      state->playerX = 0;
    }

    state->playerX += (int)(4.0f * controller->stickAverageX);
    state->playerY += (int)(4.0f * controller->stickAverageY);

    if (controller->actionDown.pressed) {
      state->playerY -= 10;
    }
  }

  draw_frame(backbuffer, state->blueOffset, state->greenOffset);
  draw_player(backbuffer, state->playerX, state->playerY);
}
