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

static inline i32 roundf32toi32(f32 value) {
  return (i32)__builtin_round(value);
}
static inline u32 roundf32tou32(f32 value) {
  return (u32)__builtin_round(value);
}

static void draw_rectangle(struct game_backbuffer *backbuffer, f32 realMinX,
                           f32 realMinY, f32 realMaxX, f32 realMaxY, f32 r,
                           f32 g, f32 b) {
  assert(realMinX < realMaxX);
  assert(realMinY < realMaxY);

  u32 minX = roundf32tou32(realMinX);
  u32 minY = roundf32tou32(realMinY);
  u32 maxX = roundf32tou32(realMaxX);
  u32 maxY = roundf32tou32(realMaxY);

  if (minX > backbuffer->width)
    minX = 0;

  if (minY > backbuffer->height)
    minY = 0;

  if (maxX > backbuffer->width)
    maxX = backbuffer->width;

  if (maxY > backbuffer->height)
    maxY = backbuffer->height;

  u8 *row = backbuffer->memory
            /* x offset */
            + (minX * backbuffer->bytes_per_pixel)
            /* y offset */
            + (minY * backbuffer->stride);

  u32 color = /* red */
      roundf32tou32(r * 255.0f) << 16
      /* green */
      | roundf32tou32(g * 255.0f) << 8
      /* blue */
      | roundf32tou32(b * 255.0f) << 0;

  for (u32 y = minY; y < maxY; y++) {
    u32 *pixel = (u32 *)row;
    for (u32 x = minX; x < maxX; x++) {
      *pixel = color;
      pixel++;
    }
    row += backbuffer->stride;
  }
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct game_state *state = memory->permanentStorage;
  if (!memory->initialized) {
    memory->initialized = 1;
  }

  for (u8 controllerIndex = 0; controllerIndex < 2; controllerIndex++) {
    struct game_controller_input *controller =
        GetController(input, controllerIndex);
    if (controller->isAnalog) {
    }

    if (controller->moveLeft.pressed) {
    }

    if (controller->moveRight.pressed) {
    }

    if (controller->moveDown.pressed) {
    }

    if (controller->moveUp.pressed) {
    }

    if (controller->actionUp.pressed) {
    }

    if (controller->actionDown.pressed) {
    }
  }

  draw_rectangle(backbuffer, 0, 0, (f32)backbuffer->width,
                 (f32)backbuffer->height, 0.0f, 0.0f, 0.0f);
  draw_rectangle(backbuffer, -10, 10, 100, 100, 1.0f, 1.0f, 1.0f);
}
