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

static inline i32 truncatef32toi32(f32 value) { return (i32)value; }

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

struct tilemap {
  f32 upperLeftX;
  f32 upperLeftY;
  f32 tileWidth;
  f32 tileHeight;
  i32 width;
  i32 height;
  u32 *tiles;
};

static struct tilemap TILEMAP_DEFAULT = {
    .upperLeftX = -30.0f,
    .upperLeftY = 0.0f,

    .tileWidth = 60.0f,
    .tileHeight = 60.0f,

    .width = 17,
    .height = 9,
    // clang-format off
    .tiles = (u32[]){
      1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
      1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
      0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
      1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1,
      1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    },
    // clang-format on
};

static inline u32 TilemapGetTileValue(struct tilemap *tilemap, i32 row,
                                      i32 column) {
  return tilemap->tiles[row * tilemap->width + column];
}

static u8 TilemapIsPointEmpty(struct tilemap *tilemap, f32 testX, f32 testY) {
  i32 playerTileX =
      truncatef32toi32((testX - tilemap->upperLeftX) / tilemap->tileWidth);
  i32 playerTileY =
      truncatef32toi32((testY - tilemap->upperLeftY) / tilemap->tileHeight);

  u8 isValid = 0;
  if (/* horizontal */
      playerTileX > 0 &&
      playerTileX < tilemap->width
      /* vertical */
      && playerTileY > 0 && playerTileY < tilemap->height) {
    return TilemapGetTileValue(tilemap, playerTileY, playerTileX) == 0;
  }

  return 0;
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct tilemap *tilemap = &TILEMAP_DEFAULT;
  struct game_state *state = memory->permanentStorage;

  f32 playerR = 1.0f;
  f32 playerG = 1.0f;
  f32 playerB = 0.0f;
  f32 playerWidth = tilemap->tileWidth * 0.75f;
  f32 playerHeight = tilemap->tileHeight;
  f32 playerLeft = state->playerX - 0.5f * playerWidth;
  f32 playerTop = state->playerY - playerHeight;

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

    f32 newPlayerX = state->playerX + input->dtPerFrame * dPlayerX;
    f32 newPlayerY = state->playerY + input->dtPerFrame * dPlayerY;

    if (TilemapIsPointEmpty(tilemap, newPlayerX + playerWidth * 0.5f,
                            newPlayerY) &&
        TilemapIsPointEmpty(tilemap, newPlayerX - playerWidth * 0.5f,
                            newPlayerY)) {
      state->playerX = newPlayerX;
      state->playerY = newPlayerY;
    }

    if (controller->actionUp.pressed) {
    }

    if (controller->actionDown.pressed) {
    }
  }

  draw_rectangle(backbuffer, 0, 0, (f32)backbuffer->width,
                 (f32)backbuffer->height, 1.0f, 0.0f, 0.0f);

  for (u8 row = 0; row < tilemap->height; row++) {
    for (u8 column = 0; column < tilemap->width; column++) {
      u32 tileid = TilemapGetTileValue(tilemap, row, column);
      f32 gray = 0.5f;
      if (tileid != 0)
        gray = 1.0f;

      f32 minX = tilemap->upperLeftX + (f32)column * tilemap->tileWidth;
      f32 minY = tilemap->upperLeftY + (f32)row * tilemap->tileHeight;
      f32 maxX = minX + tilemap->tileWidth;
      f32 maxY = minY + tilemap->tileHeight;
      draw_rectangle(backbuffer, minX, minY, maxX, maxY, gray, gray, gray);
    }
  }

  draw_rectangle(backbuffer, playerLeft, playerTop, playerLeft + playerWidth,
                 playerTop + playerHeight, playerR, playerG, playerB);
}
