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

static const f32 TILE_SIDE_IN_METERS = 1.4f;
static const u32 TILE_SIDE_IN_PIXELS = 60;
static struct world WORLD_DEFAULT = {
    .width = 2,
    .height = 2,

    .tileSideInMeters = TILE_SIDE_IN_METERS,
    .tileSideInPixels = TILE_SIDE_IN_PIXELS,
    .metersToPixels = (f32)TILE_SIDE_IN_PIXELS / TILE_SIDE_IN_METERS,

    .upperLeftX = -(f32)TILE_SIDE_IN_PIXELS * 0.5f,
    .upperLeftY = 0.0f,

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

static inline void PositionCorrectCoord(struct world *world, i32 tileCount,
                                        i32 *tilemap, i32 *tile, f32 *tileRel) {
  i32 offset = floorf32toi32(*tileRel / world->tileSideInMeters);
  *tile += offset;
  *tileRel -= (f32)offset * world->tileSideInMeters;

  assert(*tileRel >= 0);
  assert(*tileRel < world->tileSideInMeters);

  if (*tile < 0) {
    *tile += tileCount;
    *tilemap -= 1;
  }

  if (*tile >= tileCount) {
    *tile -= tileCount;
    *tilemap += 1;
  }
}

static struct position PositionCorrect(struct world *world,
                                       struct position pos) {
  struct position result = pos;

  PositionCorrectCoord(world, world->tilemapWidth, &result.tilemapX,
                       &result.tileX, &result.tileRelX);
  PositionCorrectCoord(world, world->tilemapHeight, &result.tilemapY,
                       &result.tileY, &result.tileRelY);

  return result;
}

static u8 WorldIsPointEmpty(struct world *world, struct position testPos) {

  struct position correctPos = PositionCorrect(world, testPos);
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
    state->playerPos.tilemapX = 0;
    state->playerPos.tilemapY = 0;
    state->playerPos.tileX = 3;
    state->playerPos.tileY = 3;
    state->playerPos.tileRelX = 5.0f;
    state->playerPos.tileRelY = 5.0f;

    memory->initialized = 1;
  }

  struct tilemap *tilemap = WorldGetTilemap(world, state->playerPos.tilemapX,
                                            state->playerPos.tilemapY);
  assert(tilemap && "tilemap not exits at player tilemap location");

  /* unit: meters */
  f32 playerHeight = 1.4f;
  /* unit: meters */
  f32 playerWidth = playerHeight * 0.75f;

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

    static const f32 playerSpeed = 2.0f;
    dPlayerX *= playerSpeed;
    dPlayerY *= playerSpeed;

    struct position newPlayerPos = state->playerPos;
    newPlayerPos.tileRelX += input->dtPerFrame * dPlayerX;
    newPlayerPos.tileRelY += input->dtPerFrame * dPlayerY;
    newPlayerPos = PositionCorrect(world, newPlayerPos);

    struct position left = newPlayerPos;
    left.tileRelX -= playerWidth * 0.5f;
    left = PositionCorrect(world, left);

    struct position right = newPlayerPos;
    right.tileRelX += playerWidth * 0.5f;
    right = PositionCorrect(world, right);

    if (WorldIsPointEmpty(world, left) && WorldIsPointEmpty(world, right)) {
      state->playerPos = newPlayerPos;
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

      /* player tile x, y */
      if (state->playerPos.tileX == column && state->playerPos.tileY == row) {
        gray = 0.0f;
      }

      f32 minX = world->upperLeftX + (f32)column * (f32)world->tileSideInPixels;
      f32 minY = world->upperLeftY + (f32)row * (f32)world->tileSideInPixels;
      f32 maxX = minX + (f32)world->tileSideInPixels;
      f32 maxY = minY + (f32)world->tileSideInPixels;
      draw_rectangle(backbuffer, minX, minY, maxX, maxY, gray, gray, gray);
    }
  }

  f32 playerR = 1.0f;
  f32 playerG = 1.0f;
  f32 playerB = 0.0f;

  f32 playerLeft =
      /* screen offset */
      world->upperLeftX
      /* tile */
      + (f32)state->playerPos.tileX * (f32)world->tileSideInPixels
      /* relative to tile */
      + state->playerPos.tileRelX * world->metersToPixels
      /* offset */
      - 0.5f * playerWidth * world->metersToPixels;

  f32 playerTop =
      /* screen offset */
      world->upperLeftY
      /* tile */
      + (f32)state->playerPos.tileY * (f32)world->tileSideInPixels
      /* relative to tile */
      + state->playerPos.tileRelY * world->metersToPixels
      /* offset */
      - playerHeight * world->metersToPixels;

  draw_rectangle(backbuffer,
                 /* min x, y */
                 playerLeft, playerTop,
                 /* max x */
                 playerLeft + playerWidth * world->metersToPixels,
                 /* max y */
                 playerTop + playerHeight * world->metersToPixels,
                 /* color */
                 playerR, playerG, playerB);
}
