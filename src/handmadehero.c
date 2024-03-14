#include <handmadehero/assert.h>
#include <handmadehero/handmadehero.h>

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
static const u32 CHUNK_BITS = 8;

static struct world WORLD_DEFAULT = {
    /* NOTE: this is set to using 256x256 tile chunks */
    .chunkShift = CHUNK_BITS,
    .chunkMask = (1 << CHUNK_BITS) - 1,
    .chunkDim = (1 << CHUNK_BITS),

    .tileSideInMeters = TILE_SIDE_IN_METERS,
    .tileSideInPixels = TILE_SIDE_IN_PIXELS,
    .metersToPixels = (f32)TILE_SIDE_IN_PIXELS / TILE_SIDE_IN_METERS,

    .tileChunkCountX = 1,
    .tileChunkCountY = 1,
    .tileChunks =
        (struct tile_chunk[]){
            {
                // clang-format off
                .tiles = (u32*)(u32[256][256]){
                  { 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1, },
                  { 1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0, },
                  { 1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1, },
                  { 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1, },
                  { 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1, },
                  { 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1, },
                },

                // clang-format on
            },
        },
};

static inline u32 TileChunkGetTileValue(struct world *world,
                                        struct tile_chunk *tileChunk, u32 x,
                                        u32 y) {
  assert(tileChunk);
  assert(x < world->chunkDim);
  assert(y < world->chunkDim);
  return tileChunk->tiles[y * world->chunkDim + x];
}

static inline struct tile_chunk *
WorldGetTileChunk(struct world *world, u32 tileChunkX, u32 tileChunkY) {
  if (tileChunkX > world->chunkDim)
    return 0;

  if (tileChunkY > world->chunkDim)
    return 0;

  return &world->tileChunks[tileChunkY * world->chunkDim + tileChunkY];
}

static inline void PositionCorrectCoord(struct world *world, u32 *tile,
                                        f32 *tileRel) {
  /* NOTE: World is assumed to be toroidal topology, if you step off one end
   * you come back on other.
   */

  i32 offset = roundf32toi32(*tileRel / world->tileSideInMeters);
  *tile += (u32)offset;
  *tileRel -= (f32)offset * world->tileSideInMeters;

  assert(*tileRel >= -0.5f * world->tileSideInMeters);
  assert(*tileRel < 0.5f * world->tileSideInMeters);
}

static struct position PositionCorrect(struct world *world,
                                       struct position *pos) {
  struct position result = *pos;

  PositionCorrectCoord(world, &result.absTileX, &result.tileRelX);
  PositionCorrectCoord(world, &result.absTileY, &result.tileRelY);

  return result;
}

static inline struct position_tile_chunk
PositionTileChunkGet(struct world *world, u32 absTileX, u32 absTileY) {
  struct position_tile_chunk result;

  result.tileChunkX = absTileX >> world->chunkShift;
  result.tileChunkY = absTileY >> world->chunkShift;

  result.relTileX = absTileX & world->chunkMask;
  result.relTileY = absTileY & world->chunkMask;

  return result;
}

static inline u32 TileGetValue(struct world *world, u32 absTileX,
                               u32 absTileY) {
  struct position_tile_chunk chunkPos =
      PositionTileChunkGet(world, absTileX, absTileY);
  struct tile_chunk *tileChunk =
      WorldGetTileChunk(world, chunkPos.tileChunkX, chunkPos.tileChunkY);
  assert(tileChunk);
  u32 value = TileChunkGetTileValue(world, tileChunk, chunkPos.relTileX,
                                    chunkPos.relTileY);

  return value;
}

static u8 WorldIsPointEmpty(struct world *world, struct position *testPos) {
  u32 value = TileGetValue(world, testPos->absTileX, testPos->absTileY);
  return value == 0;
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct world *world = &WORLD_DEFAULT;
  struct game_state *state = memory->permanentStorage;

  if (!memory->initialized) {
    state->playerPos.absTileX = 3;
    state->playerPos.absTileY = 3;
    state->playerPos.tileRelX = 5.0f;
    state->playerPos.tileRelY = 5.0f;

    memory->initialized = 1;
  }

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
      dPlayerY = -1.0f;
    }

    if (controller->moveUp.pressed) {
      dPlayerY = 1.0f;
    }

    f32 playerSpeed = 2.0f;
    if (controller->actionDown.pressed) {
      playerSpeed = 10.0f;
    }

    dPlayerX *= playerSpeed;
    dPlayerY *= playerSpeed;

    struct position newPlayerPos = state->playerPos;
    newPlayerPos.tileRelX += input->dtPerFrame * dPlayerX;
    newPlayerPos.tileRelY += input->dtPerFrame * dPlayerY;
    newPlayerPos = PositionCorrect(world, &newPlayerPos);

    struct position left = newPlayerPos;
    left.tileRelX -= playerWidth * 0.5f;
    left = PositionCorrect(world, &left);

    struct position right = newPlayerPos;
    right.tileRelX += playerWidth * 0.5f;
    right = PositionCorrect(world, &right);

    if (WorldIsPointEmpty(world, &left) && WorldIsPointEmpty(world, &right)) {
      state->playerPos = newPlayerPos;
    }
  }

  draw_rectangle(backbuffer, 0, 0, (f32)backbuffer->width,
                 (f32)backbuffer->height, 1.0f, 0.0f, 0.0f);

  f32 screenCenterX = 0.5f * (f32)backbuffer->width;
  f32 screenCenterY = 0.5f * (f32)backbuffer->height;

  struct position *playerPos = &state->playerPos;

  for (i32 relRow = -10; relRow < 10; relRow++) {
    for (i32 relColumn = -20; relColumn < 20; relColumn++) {
      i32 testColumn = (i32)playerPos->absTileX + relColumn;
      i32 testRow = (i32)playerPos->absTileY + relRow;

      if (testColumn < 0)
        continue;
      if (testRow < 0)
        continue;

      u32 column = (u32)testColumn;
      u32 row = (u32)testRow;

      u32 tileid = TileGetValue(world, column, row);
      f32 gray = 0.5f;
      if (tileid != 0)
        gray = 1.0f;

      /* player tile x, y */
      if (state->playerPos.absTileX == column &&
          state->playerPos.absTileY == row) {
        gray = 0.0f;
      }

      f32 centerX =
          /* screen offset */
          screenCenterX
          /* player offset */
          - state->playerPos.tileRelX * world->metersToPixels
          /* tile offset */
          + (f32)relColumn * (f32)world->tileSideInPixels;
      f32 centerY =
          /* screen offset */
          screenCenterY
          /* player offset */
          + state->playerPos.tileRelY * world->metersToPixels
          /* tile offset */
          - (f32)relRow * (f32)world->tileSideInPixels;

      f32 left = centerX - 0.5f * (f32)world->tileSideInPixels;
      f32 bottom = centerY + 0.5f * (f32)world->tileSideInPixels;
      f32 right = left + (f32)world->tileSideInPixels;
      f32 top = bottom - (f32)world->tileSideInPixels;
      draw_rectangle(backbuffer, left, top, right, bottom, gray, gray, gray);
    }
  }

  f32 playerR = 1.0f;
  f32 playerG = 1.0f;
  f32 playerB = 0.0f;

  f32 playerLeft =
      /* screen offset */
      screenCenterX
      /* offset */
      - 0.5f * playerWidth * world->metersToPixels;

  f32 playerTop =
      /* screen offset */
      screenCenterY
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
