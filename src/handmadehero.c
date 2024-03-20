#include <handmadehero/assert.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/math.h>
#include <handmadehero/random.h>

static void DrawRectangle(struct game_backbuffer *backbuffer, struct v2 min,
                          struct v2 max, f32 r, f32 g, f32 b) {
  assert(min.x < max.x);
  assert(min.y < max.y);

  i32 minX = roundf32toi32(min.x);
  i32 minY = roundf32toi32(min.y);
  i32 maxX = roundf32toi32(max.x);
  i32 maxY = roundf32toi32(max.y);

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
                       struct game_backbuffer *backbuffer, struct v2 pos,
                       i32 alignX, i32 alignY) {
  v2_sub_ref(&pos, (struct v2){
                       .x = (f32)alignX,
                       .y = (f32)alignY,
                   });

  i32 minX = roundf32toi32(pos.x);
  i32 minY = roundf32toi32(pos.y);
  i32 maxX = roundf32toi32(pos.x + (f32)bitmap->width);
  i32 maxY = roundf32toi32(pos.y + (f32)bitmap->height);

  u32 srcOffsetX = 0;
  if (minX < 0) {
    srcOffsetX = (u32)-minX;
    minX = 0;
  }

  u32 srcOffsetY = 0;
  if (minY < 0) {
    srcOffsetY = (u32)-minY;
    minY = 0;
  }

  if (maxX > (i32)backbuffer->width)
    maxX = (i32)backbuffer->width;

  if (maxY > (i32)backbuffer->height)
    maxY = (i32)backbuffer->height;

  /* bitmap file pixels goes bottom to up */
  u32 *srcRow = bitmap->pixels
                /* clipped offset */
                - srcOffsetY * bitmap->width +
                srcOffsetX
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

static inline struct entity *EntityGet(struct game_state *state, u32 index) {
  if (index >= HANDMADEHERO_ENTITY_TOTAL)
    return 0;

  return &state->entities[index];
}

static inline struct entity *EntityAdd(struct game_state *state) {
  struct entity *entity;

  entity = EntityGet(state, state->entityCount);
  state->entityCount++;
  *entity = (struct entity){};

  return entity;
}

static inline void EntityReset(struct entity *entity) {
  assert(entity);
  entity->exists = 1;

  /* set initial player position */
  entity->position.absTileX = 1;
  entity->position.absTileY = 3;
  entity->position.offset = (struct v2){.x = 5.0f, .y = 5.0f};

  /* set initial player velocity */
  entity->dPosition.x = 0;
  entity->dPosition.y = 0;
}

#if 0
static void foo() {
  struct position_tile_map oldPlayerPos = state->playerPos;

    /* if moving diagonally */
    if (ddPlayer.x != 0 && ddPlayer.y != 0) {
      /* Pythagorean theorem
       *
       *        /|
       *     d / | a
       *      /  |
       *     /___|
       *       a
       *
       *           d² = a² + a²
       *           d² = 2a²
       *      d² / 2  = a²
       *    √(d² / 2) = a
       *  √(d² . 1/2) = a
       *     d √(1/2) = a
       *
       * player must move 1 unit even when moving diagonally.
       */
      const f32 squareRoot = 0.7071067811865476f;
      v2_mul_ref(&ddPlayer, squareRoot);
    }

    /* set player speed in m/s² */
    f32 playerSpeed = 10.0f;
    if (controller->actionDown.pressed) {
      playerSpeed = 50.0f;
    }
    v2_mul_ref(&ddPlayer, playerSpeed);

    /*
     * apply friction opposite force to acceleration
     */
    v2_add_ref(&ddPlayer, v2_neg(v2_mul(state->dPlayerPos, 1.5f)));

    /* calculation of new player position
     *
     *  (position)      f(t) = 1/2 a t² + v t + p
     *     where p is old position
     *           a is acceleration
     *           t is time
     *  (velocity)     f'(t) = a t + v
     *  (acceleration) f"(t) = a
     *     acceleration coming from user
     *
     *  calculated using integral of acceleration
     *  (velocity) ∫(f"(t)) = f'(t) = a t + v
     *             where v is old velocity
     *             using integral of velocity
     *  (position) ∫(f'(t)) = 1/2 a t² + v t + p
     *             where p is old position
     *
     *        |-----|-----|-----|-----|-----|--->
     *  frame 0     1     2     3     4     5
     *        ⇐ ∆t  ⇒
     *     ∆t comes from platform layer
     */
    struct position_tile_map newPlayerPos = state->playerPos;

    // clang-format off
    /* new position */
    struct v2 playerDelta =
        /* 1/2 a t² + v t */
        v2_add(
          /* 1/2 a t² */
          v2_mul(
            /* 1/2 a */
            v2_mul(ddPlayer, 0.5f),
            /* t² */
            square(input->dtPerFrame)
          ), /* end: 1/2 a t² */
          /* v t */
          v2_mul(
            /* v */
            state->dPlayerPos,
            /* t */
            input->dtPerFrame
          ) /* end: v t */
        );

    /* 1/2 a t² + v t + p */
    v2_add_ref(&newPlayerPos.offset, playerDelta);

    /* new velocity */
    state->dPlayerPos =
      /* a t + v */
      v2_add(
        /* a t */
        v2_mul(
            /* a */
            ddPlayer,
            /* t */
            input->dtPerFrame
        ),
        /* v */
        state->dPlayerPos
      );
    // clang-format on
    newPlayerPos = PositionCorrect(tileMap, &newPlayerPos);

    struct position_tile_map left = newPlayerPos;
    v2_sub_ref(&left.offset, (struct v2){.x = playerWidth * 0.5f});
    left = PositionCorrect(tileMap, &left);

    struct position_tile_map right = newPlayerPos;
    v2_add_ref(&right.offset, (struct v2){.x = playerWidth * 0.5f});
    right = PositionCorrect(tileMap, &right);

    u8 collided = 0;
    struct position_tile_map *collisionPos;
    if (!TileMapIsPointEmpty(tileMap, &left)) {
      collided = 1;
      collisionPos = &left;
    }
    if (!TileMapIsPointEmpty(tileMap, &right)) {
      collided = 1;
      collisionPos = &right;
    }

    if (!collided)
      state->playerPos = newPlayerPos;
    else {
      /* |r| = 1 */
      struct v2 r = {0, 0};
      /* collision accoured in west of player */
      if (collisionPos->absTileX < state->playerPos.absTileX)
        r = (struct v2){1, 0};
      /* collision accoured in east of player */
      if (collisionPos->absTileX > state->playerPos.absTileX)
        r = (struct v2){-1, 0};
      /* collision accoured in north of player */
      if (collisionPos->absTileY > state->playerPos.absTileY)
        r = (struct v2){0, 1};
      /* collision accoured in south of player */
      if (collisionPos->absTileY < state->playerPos.absTileY)
        r = (struct v2){0, -1};

      // clang-format off
      state->dPlayerPos =
        /* v - vTr r */
        v2_sub(
            /* v */
            state->dPlayerPos,
            /* - vTr r */
            v2_mul(
              r,
              /* vTr */
              v2_dot(state->dPlayerPos, r)
            )
        );
      // clang-format on

#if 0
    u32 minTileX = 0;
    u32 minTileY = 0;
    u32 maxTileX = 0;
    u32 maxTileY = 0;
    u32 absTileZ = state->playerPos.absTileZ;
    struct position_tile_map bestPlayerPos = state->playerPos;
    f32 bestDistanceSq = v2_length_square(playerDelta);
    for (u32 absTileX = minTileX; absTileX != maxTileX; absTileX++) {
      for (u32 absTileY = minTileY; absTileY != maxTileY; absTileY++) {
        u32 tileValue = TileGetValue(tileMap, absTileX, absTileY, absTileZ);
        if (TileIsEmpty(tileValue)) {
          struct position_tile_map testTilePos =
              PositionTileMapCentered(absTileX, absTileY, absTileZ);

          struct v2 minCorner = v2_mul(
              (struct v2){tileMap->tileSideInMeters, tileMap->tileSideInMeters},
              -0.5f);
          struct v2 maxCorner = v2_mul(
              (struct v2){tileMap->tileSideInMeters, tileMap->tileSideInMeters},
              0.5f);

          struct position_difference relNewPlayerPos =
              PositionDifference(tileMap, &testTilePos, &newPlayerPos);
          // struct v2 testP = ClosestPointInRectangle(minCorner, maxCorner);
        }
#endif
    }
  }

  /* update player position */
  if (!PositionTileMapSameTile(&oldPlayerPos, &state->playerPos)) {
    u32 newTileValue = TileGetValue2(tileMap, &state->playerPos);

    if (newTileValue & TILE_LADDER_UP) {
      state->playerPos.absTileZ++;
    }

    if (newTileValue & TILE_LADDER_DOWN) {
      state->playerPos.absTileZ--;
    }

    assert(state->playerPos.absTileZ < tileMap->tileChunkCountZ);
  }

  if (absolute(ddPlayer.x) > absolute(ddPlayer.y)) {
    if (ddPlayer.x < 0)
      state->heroFacingDirection = BITMAP_HERO_LEFT;
    else
      state->heroFacingDirection = BITMAP_HERO_RIGHT;
  }
  else if (abs(ddPlayer.x) < abs(ddPlayer.y)) {
    if (ddPlayer.y > 0)
      state->heroFacingDirection = BITMAP_HERO_BACK;
    else
      state->heroFacingDirection = BITMAP_HERO_FRONT;
  }

}
}
#endif

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

    /* initialize controller entity hash table with invalid entities */
    for (u32 controllerIndex = 0; controllerIndex < HANDMADEHERO_CONTROLLER_COUNT; controllerIndex++) {
      state->playerIndexForController[controllerIndex] = HANDMADEHERO_ENTITY_TOTAL;
    }

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

  for (u8 controllerIndex = 0; controllerIndex < HANDMADEHERO_CONTROLLER_COUNT;
       controllerIndex++) {
    struct game_controller_input *controller =
        GetController(input, controllerIndex);
    struct entity *controlledEntity =
        EntityGet(state, state->playerIndexForController[controllerIndex]);

    if (!controlledEntity) {
      /* wait for start button pressed to enable */
      if (controller->start.pressed) {
        controlledEntity = EntityAdd(state);
        EntityReset(controlledEntity);
      }
      continue;
    }

    /* acceleration */
    struct v2 ddPlayer = {};

    /* analog controller */
    if (controller->isAnalog) {
      ddPlayer = (struct v2){
          .x = controller->stickAverageX,
          .y = controller->stickAverageY,
      };
    }

    /* digital controller */
    else {

      if (controller->moveLeft.pressed) {
        ddPlayer.x = -1.0f;
      }

      if (controller->moveRight.pressed) {
        ddPlayer.x = 1.0f;
      }

      if (controller->moveDown.pressed) {
        ddPlayer.y = -1.0f;
      }

      if (controller->moveUp.pressed) {
        ddPlayer.y = 1.0f;
      }
    }
  }

  struct entity *followedEntity = EntityGet(state, state->followedEntityIndex);

  if (followedEntity) {
    state->cameraPos.absTileZ = followedEntity->position.absTileZ;

    struct position_difference diff = PositionDifference(
        tileMap, &followedEntity->position, &state->cameraPos);

    f32 maxDiffX = (f32)tilesPerWidth * 0.5f * tileMap->tileSideInMeters;
    if (diff.dXY.x > maxDiffX)
      state->cameraPos.absTileX += tilesPerWidth;
    else if (diff.dXY.x < -maxDiffX)
      state->cameraPos.absTileX -= tilesPerWidth;

    f32 maxDiffY = (f32)tilesPerHeight * 0.5f * tileMap->tileSideInMeters;
    if (diff.dXY.y > maxDiffY)
      state->cameraPos.absTileY += tilesPerHeight;
    else if (diff.dXY.y < -maxDiffY)
      state->cameraPos.absTileY -= tilesPerHeight;
  }

  /****************************************************************
   * RENDERING
   ****************************************************************/
  DrawBitmap(&state->bitmapBackground, backbuffer, (struct v2){}, 0, 0);

  struct v2 screenCenter = {
      .x = 0.5f * (f32)backbuffer->width,
      .y = 0.5f * (f32)backbuffer->height,
  };

  /* render tiles relative to player position */
  for (i32 relRow = -10; relRow < 10; relRow++) {
    for (i32 relColumn = -20; relColumn < 20; relColumn++) {
      i32 testColumn = (i32)state->cameraPos.absTileX + relColumn;
      i32 testRow = (i32)state->cameraPos.absTileY + relRow;

      if (testColumn < 0)
        continue;
      if (testRow < 0)
        continue;

      u32 column = (u32)testColumn;
      u32 row = (u32)testRow;
      u32 plane = state->cameraPos.absTileZ;

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

      struct v2 center = {.x = screenCenter.x /* screen offset */
                               /* follow camera */
                               - state->cameraPos.offset.x * metersToPixels
                               /* tile offset */
                               + (f32)relColumn * (f32)tileSideInPixels,

                          .y = screenCenter.y /* screen offset */
                               /* follow camera */
                               + state->cameraPos.offset.y * metersToPixels
                               /* tile offset */
                               - (f32)relRow * (f32)tileSideInPixels};

      struct v2 tileSide = {
          .x = 0.5f * (f32)tileSideInPixels,
          .y = 0.5f * (f32)tileSideInPixels,
      };
      /* left top */
      struct v2 min = v2_sub(center, tileSide);
      /* right bottom */
      struct v2 max = v2_add(center, tileSide);
      DrawRectangle(backbuffer, min, max, gray, gray, gray);
    }
  }

  /* render entities */
  for (u32 entityIndex = 0; entityIndex < state->entityCount; entityIndex++) {
    struct entity *entity = EntityGet(state, entityIndex);
    if (!entity->exists)
      continue;

    struct position_difference diff =
        PositionDifference(tileMap, &entity->position, &state->cameraPos);
    /* convert from meters to pixel */
    v2_mul_ref(&diff.dXY, metersToPixels);

    f32 playerR = 1.0f;
    f32 playerG = 1.0f;
    f32 playerB = 0.0f;

    struct v2 playerGroundPoint = {
        .x = screenCenter.x + diff.dXY.x,
        .y = screenCenter.y - diff.dXY.y,
    };

    struct v2 playerLeftTop = (struct v2){
        .x = playerGroundPoint.x - 0.5f * playerWidth * metersToPixels,
        .y = playerGroundPoint.y - playerHeight * metersToPixels,
    };

    struct v2 playerWidthHeight = (struct v2){
        .x = playerWidth,
        .y = playerHeight,
    };
    v2_mul_ref(&playerWidthHeight, metersToPixels);
    struct v2 playerRightBottom = v2_add(playerLeftTop, playerWidthHeight);

    DrawRectangle(backbuffer, playerLeftTop, playerRightBottom,
                  /* color */
                  playerR, playerG, playerB);

    struct bitmap_hero *bitmap = &state->bitmapHero[entity->facingDirection];
    DrawBitmap(&bitmap->torso, backbuffer, playerGroundPoint, bitmap->alignX,
               bitmap->alignY);
    DrawBitmap(&bitmap->cape, backbuffer, playerGroundPoint, bitmap->alignX,
               bitmap->alignY);
    DrawBitmap(&bitmap->head, backbuffer, playerGroundPoint, bitmap->alignX,
               bitmap->alignY);
  }
}
