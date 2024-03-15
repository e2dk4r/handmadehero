#include <handmadehero/assert.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/math.h>
#include <handmadehero/random.h>

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

static inline u8 WorldIsPointEmpty(struct tile_map *tileMap,
                                   struct position_tile_map *testPos) {
  u32 value = TileGetValue(tileMap, testPos->absTileX, testPos->absTileY);
  return value & TILE_WALKABLE;
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct game_state *state = memory->permanentStorage;

  if (!memory->initialized) {
    state->playerPos.absTileX = 1;
    state->playerPos.absTileY = 3;
    state->playerPos.tileRelX = 5.0f;
    state->playerPos.tileRelY = 5.0f;

    /* world creation */
    void *data = memory->permanentStorage + sizeof(*state);
    memory_arena_size_t size = memory->permanentStorageSize - sizeof(*state);
    MemoryArenaInit(&state->worldArena, data, size);

    struct world *world = MemoryArenaPush(&state->worldArena, sizeof(*world));
    state->world = world;
    struct tile_map *tileMap =
        MemoryArenaPush(&state->worldArena, sizeof(*tileMap));
    world->tileMap = tileMap;

    tileMap->chunkShift = 4;
    tileMap->chunkDim = (u32)(1 << tileMap->chunkShift);
    tileMap->chunkMask = (u32)(1 << tileMap->chunkShift) - 1;

    tileMap->tileSideInMeters = 1.4f;

    tileMap->tileChunkCountX = 128;
    tileMap->tileChunkCountY = 128;
    tileMap->tileChunks =
        MemoryArenaPush(&state->worldArena, sizeof(struct tile_chunk) *
                                                tileMap->tileChunkCountX *
                                                tileMap->tileChunkCountY);
    /* generate procedural tile map */
    u32 tilesPerWidth = 17;
    u32 tilesPerHeight = 9;
    u32 screenX = 0;
    u32 screenY = 0;

    u8 isDoorLeft = 0;
    u8 isDoorRight = 0;
    u8 isDoorTop = 0;
    u8 isDoorBottom = 0;

    for (u32 screenIndex = 0; screenIndex < 100; screenIndex++) {
      u32 randomValue = RandomNumber() & 1;

      if (randomValue == 0)
        isDoorRight = 1;
      else
        isDoorTop = 1;

      for (u32 tileY = 0; tileY < tilesPerHeight; tileY++) {
        for (u32 tileX = 0; tileX < tilesPerWidth; tileX++) {

          u32 absTileX = screenX * tilesPerWidth + tileX;
          u32 absTileY = screenY * tilesPerHeight + tileY;

          u32 value = TILE_WALKABLE;

          if (tileX == 0 && (!isDoorLeft || tileY != tilesPerHeight / 2))
            value = TILE_BLOCKED;

          if (tileX == tilesPerWidth - 1 &&
              (!isDoorRight || tileY != tilesPerHeight / 2))
            value = TILE_BLOCKED;

          if (tileY == 0 && (!isDoorBottom || tileX != tilesPerWidth / 2))
            value = TILE_BLOCKED;

          if (tileY == tilesPerHeight - 1 &&
              (!isDoorTop || tileX != tilesPerWidth / 2))
            value = TILE_BLOCKED;

          TileSetValue(&state->worldArena, tileMap, absTileX, absTileY, value);
        }
      }

      isDoorLeft = isDoorRight;
      isDoorBottom = isDoorTop;

      isDoorRight = 0;
      isDoorTop = 0;

      if (randomValue == 0)
        screenX += 1;
      else
        screenY += 1;
    }

    memory->initialized = 1;
  }

  struct world *world = state->world;
  assert(world);
  struct tile_map *tileMap = world->tileMap;
  assert(tileMap);

  /* unit: meters */
  f32 playerHeight = 1.4f;
  /* unit: meters */
  f32 playerWidth = playerHeight * 0.75f;

  /* unit: pixels */
  u32 tileSideInPixels = 60;
  /* unit: meters/pixels */
  f32 metersToPixels = (f32)tileSideInPixels / tileMap->tileSideInMeters;

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

    struct position_tile_map newPlayerPos = state->playerPos;
    newPlayerPos.tileRelX += input->dtPerFrame * dPlayerX;
    newPlayerPos.tileRelY += input->dtPerFrame * dPlayerY;
    newPlayerPos = PositionCorrect(tileMap, &newPlayerPos);

    struct position_tile_map left = newPlayerPos;
    left.tileRelX -= playerWidth * 0.5f;
    left = PositionCorrect(tileMap, &left);

    struct position_tile_map right = newPlayerPos;
    right.tileRelX += playerWidth * 0.5f;
    right = PositionCorrect(tileMap, &right);

    if (WorldIsPointEmpty(tileMap, &left) &&
        WorldIsPointEmpty(tileMap, &right)) {
      state->playerPos = newPlayerPos;
    }
  }

  draw_rectangle(backbuffer, 0, 0, (f32)backbuffer->width,
                 (f32)backbuffer->height, 1.0f, 0.0f, 0.0f);

  f32 screenCenterX = 0.5f * (f32)backbuffer->width;
  f32 screenCenterY = 0.5f * (f32)backbuffer->height;

  struct position_tile_map *playerPos = &state->playerPos;

  /* render tiles relative to player position */
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

      u32 tileid = TileGetValue(tileMap, column, row);
      f32 gray = 0.0f;

      if (tileid == TILE_INVALID)
        continue;

      else if (tileid & TILE_WALKABLE)
        gray = 0.5f;

      else if (tileid & TILE_BLOCKED)
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
          - state->playerPos.tileRelX * metersToPixels
          /* tile offset */
          + (f32)relColumn * (f32)tileSideInPixels;
      f32 centerY =
          /* screen offset */
          screenCenterY
          /* player offset */
          + state->playerPos.tileRelY * metersToPixels
          /* tile offset */
          - (f32)relRow * (f32)tileSideInPixels;

      f32 left = centerX - 0.5f * (f32)tileSideInPixels;
      f32 bottom = centerY + 0.5f * (f32)tileSideInPixels;
      f32 right = left + (f32)tileSideInPixels;
      f32 top = bottom - (f32)tileSideInPixels;
      draw_rectangle(backbuffer, left, top, right, bottom, gray, gray, gray);
    }
  }

  /* render player in center of screen */
  f32 playerR = 1.0f;
  f32 playerG = 1.0f;
  f32 playerB = 0.0f;

  f32 playerLeft =
      /* screen offset */
      screenCenterX
      /* offset */
      - 0.5f * playerWidth * metersToPixels;

  f32 playerTop =
      /* screen offset */
      screenCenterY
      /* offset */
      - playerHeight * metersToPixels;

  draw_rectangle(backbuffer,
                 /* min x, y */
                 playerLeft, playerTop,
                 /* max x */
                 playerLeft + playerWidth * metersToPixels,
                 /* max y */
                 playerTop + playerHeight * metersToPixels,
                 /* color */
                 playerR, playerG, playerB);
}
