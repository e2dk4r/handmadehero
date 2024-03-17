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

static void DrawBitmap(struct bitmap *bitmap,
                       struct game_backbuffer *backbuffer, f32 realX, f32 realY,
                       i32 alignX, i32 alignY) {
  realX -= (f32)alignX;
  realY -= (f32)alignY;

  i32 minX = roundf32toi32(realX);
  i32 minY = roundf32toi32(realY);
  i32 maxX = roundf32toi32(realX + (f32)bitmap->width);
  i32 maxY = roundf32toi32(realY + (f32)bitmap->height);

  if (minX < 0)
    minX = 0;

  if (minY < 0)
    minY = 0;

  if (maxX > (i32)backbuffer->width)
    maxX = (i32)backbuffer->width;

  if (maxY > (i32)backbuffer->height)
    maxY = (i32)backbuffer->height;

  /* bitmap file pixels goes bottom to up */
  u32 *srcRow = bitmap->pixels
                /* last row offset */
                + (bitmap->height - 1) * bitmap->width;
  u8 *dstRow = backbuffer->memory
               /* x offset */
               + ((u32)minX * backbuffer->bytes_per_pixel)
               /* y offset */
               + ((u32)minY * backbuffer->stride);
  for (i32 y = minY; y < maxY; y++) {
    u32 *src = (u32 *)srcRow;
    u32 *dst = (u32 *)dstRow;

    for (i32 x = minX; x < maxX; x++) {
      // source channels
      f32 a = (f32)((*src >> 24) & 0xff) / 255.0f;
      f32 sR = (f32)((*src >> 16) & 0xff);
      f32 sG = (f32)((*src >> 8) & 0xff);
      f32 sB = (f32)((*src >> 0) & 0xff);

      // destination channels
      f32 dR = (f32)((*dst >> 16) & 0xff);
      f32 dG = (f32)((*dst >> 8) & 0xff);
      f32 dB = (f32)((*dst >> 0) & 0xff);

      /* linear blend
       *    .       .
       *    A       B
       *
       * from A to B delta is
       *    t = B - A
       *
       * for going from A to B is
       *    C = A + (B - A)
       *
       * this can be formulated where t is [0, 1]
       *    C(t) = A + t (B - A)
       *    C(t) = A + t B - t A
       *    C(t) = A (1 - t) + t B
       */

      // t is alpha and B is source because we are trying to get to it
      f32 r = dR * (1.0f - a) + a * sR;
      f32 g = dG * (1.0f - a) + a * sG;
      f32 b = dB * (1.0f - a) + a * sB;

      *dst =
          /* red */
          (u32)(r + 0.5f) << 16
          /* green */
          | (u32)(g + 0.5f) << 8
          /* blue */
          | (u32)(b + 0.5f) << 0;

      dst++;
      src++;
    }

    dstRow += backbuffer->stride;
    /* bitmap file pixels goes bottom to up */
    srcRow -= bitmap->width;
  }
}

#define BITMAP_COMPRESSION_RGB 0
#define BITMAP_COMPRESSION_BITFIELDS 3
struct __attribute__((packed)) bitmap_header {
  u16 fileType;
  u32 fileSize;
  u16 reserved1;
  u16 reserved2;
  u32 bitmapOffset;
  u32 size;
  i32 width;
  i32 height;
  u16 planes;
  u16 bitsPerPixel;
  u32 compression;
  u32 imageSize;
  u32 horzResolution;
  u32 vertResolution;
  u32 colorsPalette;
  u32 colorsImportant;
};

struct __attribute__((packed)) bitmap_header_compressed {
  struct bitmap_header header;
  u32 redMask;
  u32 greenMask;
  u32 blueMask;
};

static struct bitmap LoadBmp(pfnPlatformReadEntireFile PlatformReadEntireFile,
                             char *filename) {
  struct bitmap result = {0};

  struct read_file_result readResult = PlatformReadEntireFile(filename);
  if (readResult.size == 0) {
    return result;
  }

  struct bitmap_header *header = readResult.data;
  u32 *pixels = readResult.data + header->bitmapOffset;

  if (header->compression == BITMAP_COMPRESSION_BITFIELDS) {
    struct bitmap_header_compressed *cHeader =
        (struct bitmap_header_compressed *)header;

    i32 redShift = FindLeastSignificantBitSet((i32)cHeader->redMask);
    i32 greenShift = FindLeastSignificantBitSet((i32)cHeader->greenMask);
    i32 blueShift = FindLeastSignificantBitSet((i32)cHeader->blueMask);
    assert(redShift != greenShift);

    u32 alphaMask =
        ~(cHeader->redMask | cHeader->greenMask | cHeader->blueMask);
    i32 alphaShift = FindLeastSignificantBitSet((i32)alphaMask);

    u32 *srcDest = pixels;
    for (i32 y = 0; y < header->height; y++) {
      for (i32 x = 0; x < header->width; x++) {

        u32 value = *srcDest;
        *srcDest =
            /* blue */
            ((value >> blueShift) & 0xff) << 0
            /* green */
            | ((value >> greenShift) & 0xff) << 8
            /* red */
            | ((value >> redShift) & 0xff) << 16
            /* alpha */
            | ((value >> alphaShift) & 0xff) << 24;

        srcDest++;
      }
    }
  }

  result.pixels = pixels;

  result.width = (u32)header->width;
  if (header->width < 0)
    result.width = (u32)-header->width;

  result.height = (u32)header->height;
  if (header->height < 0)
    result.height = (u32)-header->height;

  return result;
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct game_state *state = memory->permanentStorage;

  /****************************************************************
   * INITIALIZATION
   ****************************************************************/
  static const u32 tilesPerWidth = 17;
  static const u32 tilesPerHeight = 9;
  if (!memory->initialized) {
    /* load background */
    state->bitmapBackground =
        LoadBmp(memory->PlatformReadEntireFile, "test/test_background.bmp");

    /* load hero bitmaps */
    struct bitmap_hero *bitmapHero = &state->bitmapHero[BITMAP_HERO_FRONT];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile,
                               "test/test_hero_front_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile,
                                "test/test_hero_front_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile,
                               "test/test_hero_front_cape.bmp");
    bitmapHero->alignX = 72;
    bitmapHero->alignY = 182;

    bitmapHero = &state->bitmapHero[BITMAP_HERO_BACK];
    bitmapHero->head =
        LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_back_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile,
                                "test/test_hero_back_torso.bmp");
    bitmapHero->cape =
        LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_back_cape.bmp");
    bitmapHero->alignX = 72;
    bitmapHero->alignY = 182;

    bitmapHero = &state->bitmapHero[BITMAP_HERO_LEFT];
    bitmapHero->head =
        LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_left_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile,
                                "test/test_hero_left_torso.bmp");
    bitmapHero->cape =
        LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_left_cape.bmp");
    bitmapHero->alignX = 72;
    bitmapHero->alignY = 182;

    bitmapHero = &state->bitmapHero[BITMAP_HERO_RIGHT];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile,
                               "test/test_hero_right_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile,
                                "test/test_hero_right_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile,
                               "test/test_hero_right_cape.bmp");
    bitmapHero->alignX = 72;
    bitmapHero->alignY = 182;

    /* set initial camera position */
    state->cameraPos.absTileX = 17 / 2;
    state->cameraPos.absTileY = 9 / 2;

    /* set initial player position */
    state->playerPos.absTileX = 1;
    state->playerPos.absTileY = 3;
    state->playerPos.offsetX = 5.0f;
    state->playerPos.offsetY = 5.0f;

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
    tileMap->tileChunkCountZ = 2;
    tileMap->tileChunks =
        MemoryArenaPush(&state->worldArena, sizeof(struct tile_chunk) *
                                                /* x . y . z */
                                                tileMap->tileChunkCountX *
                                                tileMap->tileChunkCountY *
                                                tileMap->tileChunkCountZ);
    /* generate procedural tile map */
    u32 screenX = 0;
    u32 screenY = 0;
    u32 absTileZ = 0;

    u8 isDoorLeft = 0;
    u8 isDoorRight = 0;
    u8 isDoorTop = 0;
    u8 isDoorBottom = 0;
    u8 isDoorUp = 0;
    u8 isDoorDown = 0;

    for (u32 screenIndex = 0; screenIndex < 100; screenIndex++) {
      u32 randomValue;

      if (isDoorUp || isDoorDown)
        randomValue = RandomNumber() % 2;
      else
        randomValue = RandomNumber() % 3;

      u8 isDoorZ = 0;
      if (randomValue == 2) {
        isDoorZ = 1;
        if (absTileZ == 0)
          isDoorUp = 1;
        else
          isDoorDown = 1;
      } else if (randomValue == 1)
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

          if (tileX == 10 && tileY == 6) {
            if (isDoorUp)
              value = TILE_LADDER_UP;

            if (isDoorDown)
              value = TILE_LADDER_DOWN;
          }

          TileSetValue(&state->worldArena, tileMap, absTileX, absTileY,
                       absTileZ, value);
        }
      }

      isDoorLeft = isDoorRight;
      isDoorBottom = isDoorTop;

      isDoorRight = 0;
      isDoorTop = 0;

      if (isDoorZ) {
        isDoorUp = !isDoorUp;
        isDoorDown = !isDoorDown;
      } else {
        isDoorUp = 0;
        isDoorDown = 0;
      }

      if (randomValue == 2)
        if (absTileZ == 0)
          absTileZ = 1;
        else
          absTileZ = 0;
      else if (randomValue == 1)
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

  /****************************************************************
   * COLLISION DETECTION
   ****************************************************************/

  /* unit: meters */
  f32 playerHeight = 1.4f;
  /* unit: meters */
  f32 playerWidth = playerHeight * 0.75f;

  /* unit: pixels */
  u32 tileSideInPixels = 60;
  /* unit: pixels/meters */
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

      if (controller->stickAverageX > 0)
        state->heroFacingDirection = BITMAP_HERO_RIGHT;
      else if (controller->stickAverageX < 0)
        state->heroFacingDirection = BITMAP_HERO_LEFT;
      if (controller->stickAverageY > 0)
        state->heroFacingDirection = BITMAP_HERO_BACK;
      else if (controller->stickAverageY < 0)
        state->heroFacingDirection = BITMAP_HERO_FRONT;
    }

    if (controller->moveLeft.pressed) {
      dPlayerX = -1.0f;
      state->heroFacingDirection = BITMAP_HERO_LEFT;
    }

    if (controller->moveRight.pressed) {
      dPlayerX = 1.0f;
      state->heroFacingDirection = BITMAP_HERO_RIGHT;
    }

    if (controller->moveDown.pressed) {
      dPlayerY = -1.0f;
      state->heroFacingDirection = BITMAP_HERO_FRONT;
    }

    if (controller->moveUp.pressed) {
      dPlayerY = 1.0f;
      state->heroFacingDirection = BITMAP_HERO_BACK;
    }

    f32 playerSpeed = 2.0f;
    if (controller->actionDown.pressed) {
      playerSpeed = 10.0f;
    }

    dPlayerX *= playerSpeed;
    dPlayerY *= playerSpeed;

    struct position_tile_map newPlayerPos = state->playerPos;
    newPlayerPos.offsetX += input->dtPerFrame * dPlayerX;
    newPlayerPos.offsetY += input->dtPerFrame * dPlayerY;
    newPlayerPos = PositionCorrect(tileMap, &newPlayerPos);

    struct position_tile_map left = newPlayerPos;
    left.offsetX -= playerWidth * 0.5f;
    left = PositionCorrect(tileMap, &left);

    struct position_tile_map right = newPlayerPos;
    right.offsetX += playerWidth * 0.5f;
    right = PositionCorrect(tileMap, &right);

    if (TileMapIsPointEmpty(tileMap, &left) &&
        TileMapIsPointEmpty(tileMap, &right)) {

      if (!PositionTileMapSameTile(&state->playerPos, &newPlayerPos)) {
        u32 newTileValue = TileGetValue2(tileMap, &newPlayerPos);

        if (newTileValue & TILE_LADDER_UP) {
          newPlayerPos.absTileZ++;
        }

        if (newTileValue & TILE_LADDER_DOWN) {
          newPlayerPos.absTileZ--;
        }

        assert(newPlayerPos.absTileZ < tileMap->tileChunkCountZ);
      }

      state->playerPos = newPlayerPos;
      state->cameraPos.absTileZ = state->playerPos.absTileZ;

      struct position_difference diff =
          PositionDifference(tileMap, &state->playerPos, &state->cameraPos);

      f32 maxDiffX = (f32)tilesPerWidth * 0.5f * tileMap->tileSideInMeters;
      if (diff.dX > maxDiffX)
        state->cameraPos.absTileX += tilesPerWidth;
      else if (diff.dX < -maxDiffX)
        state->cameraPos.absTileX -= tilesPerWidth;

      f32 maxDiffY = (f32)tilesPerHeight / 2.0f * tileMap->tileSideInMeters;
      if (diff.dY > maxDiffY)
        state->cameraPos.absTileY += tilesPerHeight;
      else if (diff.dY < -maxDiffY)
        state->cameraPos.absTileY -= tilesPerHeight;
    }
  }

  /****************************************************************
   * RENDERING
   ****************************************************************/
  DrawBitmap(&state->bitmapBackground, backbuffer, 0, 0, 0, 0);

  f32 screenCenterX = 0.5f * (f32)backbuffer->width;
  f32 screenCenterY = 0.5f * (f32)backbuffer->height;

  struct position_tile_map *playerPos = &state->playerPos;
  struct position_tile_map *cameraPos = &state->cameraPos;

  /* render tiles relative to player position */
  for (i32 relRow = -10; relRow < 10; relRow++) {
    for (i32 relColumn = -20; relColumn < 20; relColumn++) {
      i32 testColumn = (i32)cameraPos->absTileX + relColumn;
      i32 testRow = (i32)cameraPos->absTileY + relRow;

      if (testColumn < 0)
        continue;
      if (testRow < 0)
        continue;

      u32 column = (u32)testColumn;
      u32 row = (u32)testRow;
      u32 plane = cameraPos->absTileZ;

      u32 tileid = TileGetValue(tileMap, column, row, plane);
      f32 gray = 0.0f;

      if (tileid == TILE_INVALID)
        continue;

      else if (tileid & TILE_WALKABLE)
        continue;

      else if (tileid & TILE_BLOCKED)
        gray = 1.0f;

      else if (tileid & (TILE_LADDER_UP | TILE_LADDER_DOWN))
        gray = 0.25f;

      /* player tile x, y */
      if (state->cameraPos.absTileX == column &&
          state->cameraPos.absTileY == row) {
        gray = 0.0f;
      }

      f32 centerX =
          /* screen offset */
          screenCenterX
          /* follow camera */
          - state->cameraPos.offsetX * metersToPixels
          /* tile offset */
          + (f32)relColumn * (f32)tileSideInPixels;
      f32 centerY =
          /* screen offset */
          screenCenterY
          /* follow camera */
          + state->cameraPos.offsetY * metersToPixels
          /* tile offset */
          - (f32)relRow * (f32)tileSideInPixels;

      f32 left = centerX - 0.5f * (f32)tileSideInPixels;
      f32 bottom = centerY + 0.5f * (f32)tileSideInPixels;
      f32 right = left + (f32)tileSideInPixels;
      f32 top = bottom - (f32)tileSideInPixels;
      draw_rectangle(backbuffer, left, top, right, bottom, gray, gray, gray);
    }
  }

  /* render player at player position */
  struct position_difference diff =
      PositionDifference(tileMap, playerPos, cameraPos);

  f32 playerR = 1.0f;
  f32 playerG = 1.0f;
  f32 playerB = 0.0f;

  f32 playerGroundPointX =
      /* camera offset */
      screenCenterX
      /* player offset */
      + diff.dX * metersToPixels;
  f32 playerGroundPointY =
      /* camera offset */
      screenCenterY
      /* player offset */
      - diff.dY * metersToPixels;

  f32 playerLeft = playerGroundPointX
                   /* offset */
                   - 0.5f * playerWidth * metersToPixels;

  f32 playerTop = playerGroundPointY
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

  struct bitmap_hero *bitmap = &state->bitmapHero[state->heroFacingDirection];
  DrawBitmap(&bitmap->torso, backbuffer, playerGroundPointX, playerGroundPointY,
             bitmap->alignX, bitmap->alignY);
  DrawBitmap(&bitmap->cape, backbuffer, playerGroundPointX, playerGroundPointY,
             bitmap->alignX, bitmap->alignY);
  DrawBitmap(&bitmap->head, backbuffer, playerGroundPointX, playerGroundPointY,
             bitmap->alignX, bitmap->alignY);
}
