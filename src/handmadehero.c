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

static inline i32 floorf32toi32(f32 value) {
  return (i32)__builtin_floor(value);
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

static struct world WORLD_DEFAULT = {
    .width = 2,
    .height = 2,

    .upperLeftX = -30.0f,
    .upperLeftY = 0.0f,

    .tileWidth = 60.0f,
    .tileHeight = 60.0f,

    .tilemapWidth = 17,
    .tilemapHeight = 9,

    .tilemaps =
        (struct tilemap[]){
            /* north west */
            {
                // clang-format off
                .tiles = (u32[]){
                  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                  1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
                  1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                  1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1,
                  1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1,
                  1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
                  1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
                },
                // clang-format on
            },

            /* TILEMAP_NORTH_EAST */
            {
                // clang-format off
                .tiles = (u32[]){
                  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
                },
                // clang-format on
            },

            /* TILEMAP_SOUTH_WEST */
            {
                // clang-format off
                .tiles = (u32[]){
                  1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                },
                // clang-format on
            },

            /* TILEMAP_SOUTH_EAST */
            {
                // clang-format off
                .tiles = (u32[]){
                  1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                },
                // clang-format on
            },
        },
};

static inline u32 TilemapGetTileValue(struct world *world,
                                      struct tilemap *tilemap, i32 x, i32 y) {
  return tilemap->tiles[y * world->tilemapWidth + x];
}

static u8 TilemapIsPointEmpty(struct world *world, struct tilemap *tilemap,
                              i32 tileX, i32 tileY) {
  assert(tilemap);

  u8 isValid = 0;
  if (tileX < 0 || tileX >= world->tilemapWidth || tileY < 0 ||
      tileY >= world->tilemapHeight) {
    return 0;
  }

  return TilemapGetTileValue(world, tilemap, tileX, tileY) == 0;
}

static inline struct tilemap *WorldGetTilemap(struct world *world, i32 tilemapX,
                                              i32 tilemapY) {
  if (tilemapX < 0 && tilemapX > world->width)
    return 0;

  if (tilemapY < 0 && tilemapY > world->height)
    return 0;

  return &world->tilemaps[tilemapY * world->height + tilemapX];
}

static struct position_correct PositionCorrect(struct world *world,
                                               struct position_raw pos) {
  struct position_correct result;

  result.tilemapX = pos.tilemapX;
  result.tilemapY = pos.tilemapY;

  f32 x = pos.x - world->upperLeftX;
  f32 y = pos.y - world->upperLeftY;

  result.tileX = floorf32toi32(x / world->tileWidth);
  result.tileY = floorf32toi32(y / world->tileHeight);

  result.x = x - ((f32)result.tileX * world->tileWidth);
  result.y = y - ((f32)result.tileY * world->tileHeight);

  assert(result.x >= 0);
  assert(result.y >= 0);
  assert(result.x < world->tileWidth);
  assert(result.y < world->tileHeight);

  if (result.tileX < 0) {
    result.tileX += world->tilemapWidth;
    result.tilemapX--;
  }

  if (result.tileY < 0) {
    result.tileY += world->tilemapHeight;
    result.tilemapY--;
  }

  if (result.tileX >= world->tilemapWidth) {
    result.tileX -= world->tilemapWidth;
    result.tilemapX++;
  }

  if (result.tileY >= world->tilemapHeight) {
    result.tileY -= world->tilemapHeight;
    result.tilemapY++;
  }

  return result;
}

static u8 WorldIsPointEmpty(struct world *world, struct position_raw testPos) {

  struct position_correct correctPos = PositionCorrect(world, testPos);
  struct tilemap *tilemap =
      WorldGetTilemap(world, correctPos.tilemapX, correctPos.tilemapY);
  assert(tilemap);
  return TilemapIsPointEmpty(world, tilemap, correctPos.tileX,
                             correctPos.tileY);
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct world *world = &WORLD_DEFAULT;
  struct game_state *state = memory->permanentStorage;

  if (!memory->initialized) {
    state->playerX = 150.0f;
    state->playerY = 150.0f;
    state->playerTilemapX = 0;
    state->playerTilemapY = 0;

    memory->initialized = 1;
  }

  struct tilemap *tilemap =
      WorldGetTilemap(world, state->playerTilemapX, state->playerTilemapY);
  assert(tilemap && "tilemap not exits at player tilemap location");

  f32 playerR = 1.0f;
  f32 playerG = 1.0f;
  f32 playerB = 0.0f;
  f32 playerWidth = world->tileWidth * 0.75f;
  f32 playerHeight = world->tileHeight;
  f32 playerLeft = state->playerX - 0.5f * playerWidth;
  f32 playerTop = state->playerY - playerHeight;

  for (u8 controllerIndex = 0; controllerIndex < 2; controllerIndex++) {
    struct game_controller_input *controller =
        GetController(input, controllerIndex);

    /* pixels/second */
    f32 dPlayerX = 0.0f;
    /* pixels/second */
    f32 dPlayerY = 0.0f;

    if (controller->isAnalog) {
      dPlayerX += controller->stickAverageX;
      dPlayerY += controller->stickAverageY;
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

    struct position_raw center = {
        .tilemapX = state->playerTilemapX,
        .tilemapY = state->playerTilemapY,
        .x = newPlayerX,
        .y = newPlayerY,
    };
    struct position_raw left = center;
    left.x -= playerWidth * 0.5f;
    struct position_raw right = center;
    right.x += playerWidth * 0.5f;

    if (WorldIsPointEmpty(world, left) && WorldIsPointEmpty(world, right)) {
      struct position_correct newPos = PositionCorrect(world, center);
      state->playerTilemapX = newPos.tilemapX;
      state->playerTilemapY = newPos.tilemapY;

      state->playerX =
          newPos.x + (f32)newPos.tileX * world->tileWidth + world->upperLeftX;
      state->playerY =
          newPos.y + (f32)newPos.tileY * world->tileHeight + world->upperLeftY;
    }

    if (controller->actionUp.pressed) {
    }

    if (controller->actionDown.pressed) {
    }
  }

  draw_rectangle(backbuffer, 0, 0, (f32)backbuffer->width,
                 (f32)backbuffer->height, 1.0f, 0.0f, 0.0f);

  for (u8 row = 0; row < world->tilemapHeight; row++) {
    for (u8 column = 0; column < world->tilemapWidth; column++) {
      u32 tileid = TilemapGetTileValue(world, tilemap, column, row);
      f32 gray = 0.5f;
      if (tileid != 0)
        gray = 1.0f;

      f32 minX = world->upperLeftX + (f32)column * world->tileWidth;
      f32 minY = world->upperLeftY + (f32)row * world->tileHeight;
      f32 maxX = minX + world->tileWidth;
      f32 maxY = minY + world->tileHeight;
      draw_rectangle(backbuffer, minX, minY, maxX, maxY, gray, gray, gray);
    }
  }

  draw_rectangle(backbuffer, playerLeft, playerTop, playerLeft + playerWidth,
                 playerTop + playerHeight, playerR, playerG, playerB);
}
