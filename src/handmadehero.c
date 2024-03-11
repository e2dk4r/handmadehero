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

  i32 minX = roundf32toi32(realMinX);
  i32 minY = roundf32toi32(realMinY);
  i32 maxX = roundf32toi32(realMaxX);
  i32 maxY = roundf32toi32(realMaxY);

  if (minX < 0)
    minX = 0;

  if (minY < 0)
    minY = 0;

  if (maxX > (i32)backbuffer->width)
    maxX = (i32)backbuffer->width;

  if (maxY > (i32)backbuffer->height)
    maxY = (i32)backbuffer->height;

  u8 *row = backbuffer->memory
            /* x offset */
            + ((u32)minX * backbuffer->bytes_per_pixel)
            /* y offset */
            + ((u32)minY * backbuffer->stride);

  u32 color = /* red */
      roundf32tou32(r * 255.0f) << 16
      /* green */
      | roundf32tou32(g * 255.0f) << 8
      /* blue */
      | roundf32tou32(b * 255.0f) << 0;

  for (i32 y = minY; y < maxY; y++) {
    u32 *pixel = (u32 *)row;
    for (i32 x = minX; x < maxX; x++) {
      *pixel = color;
      pixel++;
    }
    row += backbuffer->stride;
  }
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct game_state *state = memory->permanentStorage;
  if (!memory->initialized) {
    state->playerX = 150.0f;
    state->playerY = 150.0f;

    memory->initialized = 1;
  }

  for (u8 controllerIndex = 0; controllerIndex < 2; controllerIndex++) {
    struct game_controller_input *controller =
        GetController(input, controllerIndex);

    /* pixels/second */
    f32 dPlayerX = 0.0f;
    /* pixels/second */
    f32 dPlayerY = 0.0f;

    if (controller->isAnalog) {
      dPlayerX += 2 * controller->stickAverageX;
      dPlayerY += 2 * controller->stickAverageY;
    }

    if (controller->moveLeft.pressed) {
      dPlayerX = -1.0f;
    }

    if (controller->moveRight.pressed) {
      dPlayerX = 1.0f;
    }

    if (controller->moveDown.pressed) {
      dPlayerY = 1.0f;
    }

    if (controller->moveUp.pressed) {
      dPlayerY = -1.0f;
    }

    dPlayerX *= 128.0f;
    dPlayerY *= 128.0f;

    state->playerX += input->dtPerFrame * dPlayerX;
    state->playerY += input->dtPerFrame * dPlayerY;

    if (controller->actionUp.pressed) {
    }

    if (controller->actionDown.pressed) {
    }
  }

  u32 tilemap[9][17] = {
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
      {1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1},
      {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1},
      {0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
      {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
      {1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
      {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
  };

  draw_rectangle(backbuffer, 0, 0, (f32)backbuffer->width,
                 (f32)backbuffer->height, 1.0f, 0.0f, 0.0f);

  f32 upperLeftX = -30.0f;
  f32 upperLeftY = 0.0f;
  f32 tileWidth = 60.0f;
  f32 tileHeight = 60.0f;

  for (u8 row = 0; row < 9; row++) {
    for (u8 column = 0; column < 17; column++) {
      u32 tileid = tilemap[row][column];
      f32 gray = 0.5f;
      if (tileid != 0)
        gray = 1.0f;

      f32 minX = upperLeftX + (f32)column * tileWidth;
      f32 minY = upperLeftY + (f32)row * tileHeight;
      f32 maxX = minX + tileWidth;
      f32 maxY = minY + tileHeight;
      draw_rectangle(backbuffer, minX, minY, maxX, maxY, gray, gray, gray);
    }
  }

  f32 playerR = 1.0f;
  f32 playerG = 1.0f;
  f32 playerB = 0.0f;
  f32 playerWidth = tileWidth * 0.75f;
  f32 playerHeight = tileHeight;
  f32 playerLeft = state->playerX - 0.5f * playerWidth;
  f32 playerTop = state->playerY - playerHeight;
  draw_rectangle(backbuffer, playerLeft, playerTop, playerLeft + playerWidth,
                 playerTop + playerHeight, playerR, playerG, playerB);
}
