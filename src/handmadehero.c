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
static void DrawBitmap2(struct bitmap *bitmap,
                        struct game_backbuffer *backbuffer, struct v2 pos,
                        i32 alignX, i32 alignY, f32 cAlpha) {
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
      a *= cAlpha;

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

static void DrawBitmap(struct bitmap *bitmap,
                       struct game_backbuffer *backbuffer, struct v2 pos,
                       i32 alignX, i32 alignY) {
  DrawBitmap2(bitmap, backbuffer, pos, alignX, alignY, 1.0f);
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
  /* index 0 is reserved for null */
  if (index == 0)
    return 0;

  if (index >= state->entityCount)
    return 0;

  struct entity *entity = &state->entities[index];

  entity->dormant = &state->entityDormants[index];
  entity->low = &state->entityLows[index];
  entity->high = &state->entityHighs[index];

  return entity;
}

static inline u32 EntityAdd(struct game_state *state) {
  u32 entityIndex = state->entityCount;
  assert(entityIndex < HANDMADEHERO_ENTITY_TOTAL);

  state->entityResidences[entityIndex] = ENTITY_RESIDENCE_DORMANT;
  state->entityHighs[entityIndex] = (struct entity_high){};
  state->entityLows[entityIndex] = (struct entity_low){};
  state->entityDormants[entityIndex] = (struct entity_dormant){};
  state->entities[entityIndex] = (struct entity){
      .collides = 1,
  };

  state->entityCount++;

  return entityIndex;
}

static inline void EntityChangeResidence(struct game_state *state,
                                         struct entity *entity, u8 residence) {
  if (residence & ENTITY_RESIDENCE_HIGH) {
    if (!(entity->residence & ENTITY_RESIDENCE_HIGH)) {
      /* if transitioning to high:
       * - map the entity to camera space
       */

      struct position_difference diff = PositionDifference(
          state->world->tileMap, &entity->dormant->position, &state->cameraPos);

      entity->high->position = diff.dXY;
      entity->high->dPosition = (struct v2){0, 0};
      entity->high->absTileZ = entity->dormant->position.absTileZ;
      entity->high->facingDirection = 0;
    }
  }

  entity->residence = residence;
}

static inline void EntityReset(struct entity *entity) {
  assert(entity);

  /* set initial player position */
  entity->dormant->position.absTileX = 1;
  entity->dormant->position.absTileY = 3;
  entity->dormant->position.offset = (struct v2){.x = 0.0f, .y = 0.0f};

  /* set initial player velocity */
  entity->high->dPosition.x = 0;
  entity->high->dPosition.y = 0;

  /* set player size */
  entity->dormant->height = 0.5f;
  entity->dormant->width = 1.0f;
}

static u8 WallTest(f32 *tMin, f32 wallX, f32 relX, f32 relY, f32 deltaX,
                   f32 deltaY, f32 minY, f32 maxY) {
  const f32 tEpsilon = 0.001f;
  u8 collided = 0;

  /* no movement, no sweet */
  if (deltaX == 0)
    goto exit;

  f32 tResult = (wallX - relX) / deltaX;
  if (tResult < 0)
    goto exit;
  /* do not care, if entity needs to go back to hit */
  if (tResult >= *tMin)
    goto exit;

  f32 y = relY + tResult * deltaY;
  if (y < minY)
    goto exit;
  if (y > maxY)
    goto exit;

  *tMin = maximum(0.0f, tResult - tEpsilon);
  collided = 1;

exit:
  return collided;
}

static void PlayerMove(struct game_state *state, struct entity *entity, f32 dt,
                       struct v2 ddPosition) {

  /*****************************************************************
   * CORRECTING ACCELERATION
   *****************************************************************/
  f32 ddPositionLength = v2_length_square(ddPosition);
  /*
   * scale down acceleration to unit vector
   * fixes moving diagonally √2 times faster
   */
  if (ddPositionLength > 1.0f) {
    v2_mul_ref(&ddPosition, 1 / square_root(ddPositionLength));
  }

  /* set player speed in m/s² */
  f32 playerSpeed = 50.0f;
  v2_mul_ref(&ddPosition, playerSpeed);

  /*
   * apply friction opposite force to acceleration
   */
  v2_add_ref(&ddPosition, v2_neg(v2_mul(entity->high->dPosition, 8.0f)));

  /*****************************************************************
   * CALCULATION OF NEW PLAYER POSITION
   *****************************************************************/

  struct v2 oldPosition = entity->high->position;

  /*
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
  // clang-format off
  struct v2 deltaPosition =
      /* 1/2 a t² + v t */
      v2_add(
        /* 1/2 a t² */
        v2_mul(
          /* 1/2 a */
          v2_mul(ddPosition, 0.5f),
          /* t² */
          square(dt)
        ), /* end: 1/2 a t² */
        /* v t */
        v2_mul(
          /* v */
          entity->high->dPosition,
          /* t */
          dt
        ) /* end: v t */
      );
  // clang-format on
  /* 1/2 a t² + v t + p */
  struct v2 newPosition = v2_add(oldPosition, deltaPosition);

  /* new velocity */
  // clang-format off
  entity->high->dPosition =
    /* a t + v */
    v2_add(
      /* a t */
      v2_mul(
          /* a */
          ddPosition,
          /* t */
          dt
      ),
      /* v */
      entity->high->dPosition
    );
  // clang-format on

  /*****************************************************************
   * COLLUSION DETECTION
   *****************************************************************/
  struct entity *hitEntity = 0;
  f32 tRemaining = 1.0f;
  for (u32 iteration = 0; iteration < 4 && tRemaining > 0.0f; iteration++) {
    f32 tMin = tRemaining;
    struct v2 wallNormal = {};

    for (u32 entityIndex = 1; entityIndex < state->entityCount; entityIndex++) {
      struct entity *testEntity = EntityGet(state, entityIndex);
      assert(testEntity);

      if (!(testEntity->residence & ENTITY_RESIDENCE_HIGH))
        continue;

      if (!testEntity->collides)
        continue;

      struct v2 diameter = {
          .x = testEntity->dormant->width + entity->dormant->width,
          .y = testEntity->dormant->height + entity->dormant->height,
      };

      struct v2 minCorner = v2_mul(diameter, -0.5f);
      struct v2 maxCorner = v2_mul(diameter, 0.5f);

      struct v2 rel =
          v2_sub(entity->high->position, testEntity->high->position);

      /* test all 4 walls and take minimum t. */
      if (WallTest(&tMin, minCorner.x, rel.x, rel.y, deltaPosition.x,
                   deltaPosition.y, minCorner.y, maxCorner.y)) {
        wallNormal = (struct v2){.x = -1, .y = 0};
        hitEntity = testEntity;
      }

      if (WallTest(&tMin, maxCorner.x, rel.x, rel.y, deltaPosition.x,
                   deltaPosition.y, minCorner.y, maxCorner.y)) {
        wallNormal = (struct v2){.x = 1, .y = 0};
        hitEntity = testEntity;
      }

      if (WallTest(&tMin, minCorner.y, rel.y, rel.x, deltaPosition.y,
                   deltaPosition.x, minCorner.x, maxCorner.x)) {
        wallNormal = (struct v2){.x = 0, .y = -1};
        hitEntity = testEntity;
      }

      if (WallTest(&tMin, maxCorner.y, rel.y, rel.x, deltaPosition.y,
                   deltaPosition.x, minCorner.x, maxCorner.x)) {
        wallNormal = (struct v2){.x = 0, .y = 1};
        hitEntity = testEntity;
      }
    }

    /* p' = tMin (1/2 a t² + v t) + p */
    v2_add_ref(&entity->high->position, v2_mul(deltaPosition, tMin));

    /*****************************************************************
     * COLLUSION HANDLING
     *****************************************************************/
    if (hitEntity) {
      /*
       * add gliding to velocity
       */
      // clang-format off
      /* v' = v - vTr r */
      v2_sub_ref(&entity->high->dPosition,
          /* vTr r */
          v2_mul(
            /* r */
            wallNormal,
            /* vTr */
            v2_dot(entity->high->dPosition, wallNormal)
          )
      );

      v2_sub_ref(&deltaPosition,
          /* pTr r */
          v2_mul(
            /* r */
            wallNormal,
            /* pTr */
            v2_dot(deltaPosition, wallNormal)
          )
      );

      // clang-format on
      entity->high->absTileZ += entity->dormant->dAbsTileZ;
    }

    tRemaining -= tMin * tRemaining;
  }

  /*****************************************************************
   * VISUAL CORRECTIONS
   *****************************************************************/
  /* use player velocity to face the direction */

  if (entity->high->dPosition.x == 0.0f && entity->high->dPosition.y == 0.0f)
    ;
  else if (absolute(entity->high->dPosition.x) >
           absolute(entity->high->dPosition.y)) {
    if (entity->high->dPosition.x < 0)
      entity->high->facingDirection = BITMAP_HERO_LEFT;
    else
      entity->high->facingDirection = BITMAP_HERO_RIGHT;
  } else {
    if (entity->high->dPosition.y > 0)
      entity->high->facingDirection = BITMAP_HERO_BACK;
    else
      entity->high->facingDirection = BITMAP_HERO_FRONT;
  }

  /* always write back into tile space */
  entity->dormant->position = PositionMapIntoTilesSpace(
      state->world->tileMap, &state->cameraPos, entity->high->position);
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

    /* load shadow */
    state->bitmapShadow =
        LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_shadow.bmp");

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
    state->cameraPos.absTileX = tilesPerWidth / 2;
    state->cameraPos.absTileY = tilesPerHeight / 2;

    /* use entity with 0 index as null */
    EntityAdd(state);

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

    for (u32 screenIndex = 0; screenIndex < 1; screenIndex++) {
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
   * CONTROLLER INPUT HANDLING
   ****************************************************************/

  for (u8 controllerIndex = 0; controllerIndex < HANDMADEHERO_CONTROLLER_COUNT;
       controllerIndex++) {
    struct game_controller_input *controller =
        GetController(input, controllerIndex);
    struct entity *controlledEntity =
        EntityGet(state, state->playerIndexForController[controllerIndex]);

    /* if there is no entity associated with this controller */
    if (!controlledEntity) {
      /* wait for start button pressed to enable */
      if (controller->start.pressed) {
        u32 entityIndex = EntityAdd(state);
        controlledEntity = EntityGet(state, entityIndex);
        assert(entityIndex < HANDMADEHERO_ENTITY_TOTAL);
        state->playerIndexForController[controllerIndex] = entityIndex;
        EntityReset(controlledEntity);
        EntityChangeResidence(state, controlledEntity, ENTITY_RESIDENCE_HIGH);

        if (state->followedEntityIndex == 0) {
          state->followedEntityIndex = entityIndex;
        }
      }
      continue;
    }

    if (controlledEntity->residence == ENTITY_RESIDENCE_NONEXISTENT)
      continue;

    /* acceleration */
    struct v2 ddPosition = {};

    /* analog controller */
    if (controller->isAnalog) {
      ddPosition = (struct v2){
          .x = controller->stickAverageX,
          .y = controller->stickAverageY,
      };
    }

    /* digital controller */
    else {

      if (controller->moveLeft.pressed) {
        ddPosition.x = -1.0f;
      }

      if (controller->moveRight.pressed) {
        ddPosition.x = 1.0f;
      }

      if (controller->moveDown.pressed) {
        ddPosition.y = -1.0f;
      }

      if (controller->moveUp.pressed) {
        ddPosition.y = 1.0f;
      }
    }

    if (controller->actionUp.pressed) {
      controlledEntity->high->dZ = 3.0f;
    }

    PlayerMove(state, controlledEntity, input->dtPerFrame, ddPosition);
  }

  /* sync camera with followed entity */
  struct v2 entityOffsetPerFrame = {};
  struct entity *followedEntity = EntityGet(state, state->followedEntityIndex);
  if (followedEntity && followedEntity->residence & ENTITY_RESIDENCE_HIGH) {
    struct position_tile_map oldCameraPos = state->cameraPos;

    state->cameraPos.absTileZ = followedEntity->dormant->position.absTileZ;

    f32 maxDiffX = (f32)tilesPerWidth * 0.5f * tileMap->tileSideInMeters;
    if (followedEntity->high->position.x > maxDiffX)
      state->cameraPos.absTileX += tilesPerWidth;
    else if (followedEntity->high->position.x < -maxDiffX)
      state->cameraPos.absTileX -= tilesPerWidth;

    f32 maxDiffY = (f32)tilesPerHeight * 0.5f * tileMap->tileSideInMeters;
    if (followedEntity->high->position.y > maxDiffY)
      state->cameraPos.absTileY += tilesPerHeight;
    else if (followedEntity->high->position.y < -maxDiffY)
      state->cameraPos.absTileY -= tilesPerHeight;

    struct position_difference diff =
        PositionDifference(tileMap, &state->cameraPos, &oldCameraPos);
    /*
     *  when camera moves,
     *  negative of camera offset must be applied to entites
     *  so that they are not moving with camera
     * ________________ ________________
     * |              | |              |
     * |              | |              |
     * |   x         x| |x'            |
     * |       c      | |       c'     |
     * |              | |              |
     * |              | |              |
     * |______________| |______________|
     */
    entityOffsetPerFrame = v2_neg(diff.dXY);
  }

  /****************************************************************
   * RENDERING
   ****************************************************************/
  /* unit: pixels */
  const u32 tileSideInPixels = 60;
  /* unit: pixels/meters */
  const f32 metersToPixels = (f32)tileSideInPixels / tileMap->tileSideInMeters;

  DrawBitmap(&state->bitmapBackground, backbuffer, (struct v2){}, 0, 0);

  struct v2 screenCenter = {
      .x = 0.5f * (f32)backbuffer->width,
      .y = 0.5f * (f32)backbuffer->height,
  };

  /* render tiles relative to camera position */
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

      struct v2 tileOffset = {
          .x = (f32)relColumn,
          .y = (f32)-relRow,
      };
      v2_mul_ref(&tileOffset, (f32)tileSideInPixels);

      struct v2 cameraOffset = state->cameraPos.offset;
      cameraOffset.y *= -1;
      v2_mul_ref(&cameraOffset, metersToPixels);

      struct v2 center = v2_add(screenCenter, v2_add(cameraOffset, tileOffset));

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
  for (u32 entityIndex = 1; entityIndex < state->entityCount; entityIndex++) {
    struct entity *entity = &state->entities[entityIndex];
    if (!(entity->residence & ENTITY_RESIDENCE_HIGH))
      continue;

    v2_add_ref(&entity->high->position, entityOffsetPerFrame);

    f32 playerR = 1.0f;
    f32 playerG = 1.0f;
    f32 playerB = 0.0f;

    f32 ddZ = -9.8f;
    entity->high->z +=
        /* 1/2 a t² */
        0.5f * ddZ * square(input->dtPerFrame)
        /* + v t */
        + entity->high->dZ * input->dtPerFrame;
    entity->high->dZ +=
        /* a t */
        ddZ * input->dtPerFrame;
    if (entity->high->z < 0)
      entity->high->z = 0;
    f32 z = entity->high->z * metersToPixels;

    f32 cAlphaShadow = 1.0f - entity->high->z;
    if (cAlphaShadow < 0.0f)
      cAlphaShadow = 0.0f;

    struct v2 playerScreenPosition = entity->high->position;
    /* screen's coordinate system uses y values inverse,
     * so that means going up in space means negative y values
     */
    playerScreenPosition.y *= -1;
    v2_mul_ref(&playerScreenPosition, metersToPixels);

    struct v2 playerGroundPoint = v2_add(screenCenter, playerScreenPosition);

    // struct v2 playerWidthHeight = (struct v2){
    //     .x = entity->dormant->width,
    //     .y = entity->dormant->height,
    // };
    // v2_mul_ref(&playerWidthHeight, metersToPixels);

    // struct v2 playerLeftTop =
    //     v2_sub(playerGroundPoint, v2_mul(playerWidthHeight, 0.5f));
    // struct v2 playerRightBottom = v2_add(playerLeftTop, playerWidthHeight);

    struct bitmap_hero *bitmap =
        &state->bitmapHero[entity->high->facingDirection];

    DrawBitmap2(&state->bitmapShadow, backbuffer, playerGroundPoint,
                bitmap->alignX, bitmap->alignY, cAlphaShadow);
    playerGroundPoint.y -= z;

    DrawBitmap(&bitmap->torso, backbuffer, playerGroundPoint, bitmap->alignX,
               bitmap->alignY);
    DrawBitmap(&bitmap->cape, backbuffer, playerGroundPoint, bitmap->alignX,
               bitmap->alignY);
    DrawBitmap(&bitmap->head, backbuffer, playerGroundPoint, bitmap->alignX,
               bitmap->alignY);
  }
}
