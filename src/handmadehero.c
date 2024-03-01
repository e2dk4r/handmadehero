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

void GameUpdateAndRender(struct game_input *input,
                         struct game_backbuffer *backbuffer) {
  static int blueOffset = 0;
  static int greenOffset = 0;

  for (u8 controllerIndex = 0; controllerIndex < 2; controllerIndex++) {
    struct game_controller_input *controller =
        &input->controllers[controllerIndex];
    if (controller->isAnalog) {
      blueOffset += (int)(4.0f * controller->endY);
    }

    if (controller->left.pressed) {
      blueOffset -= 1;
    }

    if (controller->right.pressed) {
      blueOffset += 1;
    }

    if (controller->down.pressed) {
      greenOffset += 1;
    }

    if (controller->up.pressed) {
      greenOffset -= 1;
    }
  }

  draw_frame(backbuffer, blueOffset, greenOffset);
}
