#include <handmadehero/assert.h>
#include <handmadehero/handmadehero.h>

static void draw_frame(struct game_backbuffer *backbuffer, int offsetX,
                       int offsetY) {
  u8 *row = backbuffer->memory;
  for (u32 y = 0; y < backbuffer->height; y++) {
    u8 *pixel = row;
    for (u32 x = 0; x < backbuffer->width; x++) {
      *pixel = (u8)(x + offsetX);
      pixel++;

      *pixel = (u8)(y + offsetY);
      pixel++;

      *pixel = 0x00;
      pixel++;

      *pixel = 0x00;
      pixel++;
    }
    row += backbuffer->stride;
  }
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct game_state *state = memory->permanentStorage;
  if (!memory->initialized) {
    state->blueOffset = 0;
    state->greenOffset = 0;
    memory->initialized = 1;
  }

  for (u8 controllerIndex = 0; controllerIndex < 2; controllerIndex++) {
    struct game_controller_input *controller =
        GetController(input, controllerIndex);
    if (controller->isAnalog) {
      state->blueOffset += (int)(4.0f * controller->endX);
      state->greenOffset += (int)(4.0f * controller->endY);
    }

    if (controller->left.pressed) {
      state->blueOffset -= 1;
    }

    if (controller->right.pressed) {
      state->blueOffset += 1;
    }

    if (controller->down.pressed) {
      state->greenOffset += 1;
    }

    if (controller->up.pressed) {
      state->greenOffset -= 1;
    }
  }

  draw_frame(backbuffer, state->blueOffset, state->greenOffset);
}
