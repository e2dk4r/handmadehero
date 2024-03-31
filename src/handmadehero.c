#include "handmadehero/world.h"
#include <handmadehero/assert.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/math.h>
#include <handmadehero/random.h>

static const u32 TILES_PER_WIDTH = 17;
static const u32 TILES_PER_HEIGHT = 9;

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

static inline struct entity_low *EntityLowGet(struct game_state *state,
                                              u32 index) {
  struct entity_low *result = 0;
  /* index 0 is reserved for null */
  if (index == 0)
    return result;

  if (index >= state->entityLowCount)
    return result;

  result = &state->entityLows[index];
  return result;
}
static inline struct entity_high *EntityHighGet(struct game_state *state,
                                                u32 index) {
  struct entity_high *result = 0;
  /* index 0 is reserved for null */
  if (index == 0)
    return result;

  if (index >= state->entityHighCount)
    return result;

  result = &state->entityHighs[index];
  return result;
}

static inline u32 EntityLowAdd(struct game_state *state, u8 type,
                               struct world_position *position) {
  u32 entityLowIndex = state->entityLowCount;
  assert(entityLowIndex < HANDMADEHERO_ENTITY_LOW_TOTAL);

  struct entity_low *entityLow = state->entityLows + entityLowIndex;
  assert(entityLow);
  *entityLow = (struct entity_low){};
  entityLow->collides = 1;
  entityLow->type = type;

  if (position) {
    entityLow->position = *position;
    EntityChangeLocation(&state->worldArena, state->world, entityLowIndex, 0,
                         position);
  }

  state->entityLowCount++;

  return entityLowIndex;
}

static inline void EntityMakeHighFreq(struct game_state *state, u32 lowIndex) {
  struct entity_low *entityLow = EntityLowGet(state, lowIndex);
  assert(entityLow);
  /* if it is already in high freq set */
  if (entityLow->highIndex != 0)
    return;

  entityLow->highIndex = state->entityHighCount;
  if (entityLow->highIndex == HANDMADEHERO_ENTITY_HIGH_TOTAL)
    InvalidCodePath;

  struct entity_high *entityHigh = &state->entityHighs[entityLow->highIndex];
  *entityHigh = (struct entity_high){};
  entityHigh->lowIndex = lowIndex;

  /* map the entity to camera space */
  struct world_difference diff =
      WorldPositionSub(state->world, &entityLow->position, &state->cameraPos);

  entityHigh->position = diff.dXY;
  entityHigh->dPosition = (struct v2){0, 0};
  entityHigh->chunkZ = entityLow->position.chunkZ;
  entityHigh->facingDirection = 0;

  state->entityHighCount++;
}

static inline void EntityMakeLowFreq(struct game_state *state, u32 lowIndex) {
  struct entity_low *entityLow = EntityLowGet(state, lowIndex);
  assert(entityLow);

  /* if it is already in low freq set */
  if (entityLow->highIndex == 0)
    return;

  /*
   * if deleted index is last in high freq set
   * before:
   *   high |  0 |  1 |  2 |  3 |  4x |
   *           ▲    ▲    ▲    ▲    ▲
   *   low  | 10 | 20 | 30 | 40 | 50x |
   *   x will be removed from high freq set
   *
   * after:
   *   high |  0 |  1 |  2 |  3 |  4 |
   *           ▲    ▲    ▲    ▲
   *   low  | 10 | 20 | 30 | 40 | 50 |
   ****************************************************************
   * if deleted index is not last in high freq set
   * before:
   *   high |  0 |  1 |  2x |  3 |  4 |
   *           ▲    ▲     ▲    ▲    ▲
   *   low  | 10 | 20 | 30x | 40 | 50 |
   *   x will be removed from high freq set
   *
   * after:
   *   high |  0 |  1 |  2 |  3 |  4 |
   *           ▲    ▲    ▲    ▲
   *           ┃    ┃    ┃    ┃
   *           ┃    ┃    ┗━━━━╋━━━━┓
   *   low  | 10 | 20 | 30 | 40 | 50 |
   */
  u32 lastHighIndex = state->entityHighCount - 1;
  assert(lastHighIndex < HANDMADEHERO_ENTITY_HIGH_TOTAL);
  if (entityLow->highIndex != lastHighIndex) {
    struct entity_high *lastHighEntity = &state->entityHighs[lastHighIndex];
    struct entity_high *deletedEntity =
        &state->entityHighs[entityLow->highIndex];

    *deletedEntity = *lastHighEntity;
    state->entityLows[lastHighEntity->lowIndex].highIndex =
        entityLow->highIndex;
  }

  entityLow->highIndex = 0;
  state->entityHighCount--;
}

static inline void EntityPlayerReset(struct game_state *state, u32 lowIndex) {
  assert(lowIndex < state->entityLowCount);

  struct entity_low *entityLow = EntityLowGet(state, lowIndex);
  assert(entityLow);

  /* set player size */
  entityLow->height = 0.5f;
  entityLow->width = 1.0f;

  struct entity_high *highEntity = EntityHighGet(state, entityLow->highIndex);
  if (highEntity) {
    /* set initial player velocity */
    highEntity->dPosition.x = 0;
    highEntity->dPosition.y = 0;
  }
}

static inline u32 EntityPlayerAdd(struct game_state *state) {
  struct world_position *entityPosition = &state->cameraPos;
  u32 lowIndex = EntityLowAdd(state, ENTITY_TYPE_HERO, entityPosition);
  struct entity_low *entity = EntityLowGet(state, lowIndex);
  assert(entity);
  EntityPlayerReset(state, lowIndex);
  EntityMakeHighFreq(state, lowIndex);

  /* if followed entity, does not exits */
  if (state->followedEntityIndex == 0)
    state->followedEntityIndex = lowIndex;

  return lowIndex;
}

static inline u32 EntityWallAdd(struct game_state *state, u32 absTileX,
                                u32 absTileY, u32 absTileZ) {
  struct world_position entityPosition =
      ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  u32 entityIndex = EntityLowAdd(state, ENTITY_TYPE_WALL, &entityPosition);
  struct entity_low *entityLow = EntityLowGet(state, entityIndex);
  assert(entityLow);

  entityLow->height = state->world->tileSideInMeters;
  entityLow->width = state->world->tileSideInMeters;

  return entityIndex;
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

static void PlayerMove(struct game_state *state, u32 entityLowIndex, f32 dt,
                       struct v2 ddPosition) {
  struct entity_low *entityLow = EntityLowGet(state, entityLowIndex);
  assert(entityLow);
  struct entity_high *entityHigh = EntityHighGet(state, entityLow->highIndex);
  assert(entityHigh);

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
  v2_add_ref(&ddPosition, v2_neg(v2_mul(entityHigh->dPosition, 8.0f)));

  /*****************************************************************
   * CALCULATION OF NEW PLAYER POSITION
   *****************************************************************/

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
          entityHigh->dPosition,
          /* t */
          dt
        ) /* end: v t */
      );
  // clang-format on

  /* new velocity */
  // clang-format off
  entityHigh->dPosition =
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
      entityHigh->dPosition
    );
  // clang-format on

  /*****************************************************************
   * COLLUSION DETECTION
   *****************************************************************/
  struct entity_low *hitEntityLow = 0;
  struct v2 desiredPosition = v2_add(entityHigh->position, deltaPosition);
  for (u32 iteration = 0; iteration < 4; iteration++) {
    f32 tMin = 1.0f;
    struct v2 wallNormal = {};

    for (u32 entityHighIndex = 1; entityHighIndex < state->entityHighCount;
         entityHighIndex++) {
      struct entity_high *testEntityHigh =
          EntityHighGet(state, entityHighIndex);
      assert(testEntityHigh);
      struct entity_low *testEntityLow =
          EntityLowGet(state, testEntityHigh->lowIndex);
      assert(testEntityLow);

      if (!testEntityLow->collides)
        continue;

      struct v2 diameter = {
          .x = testEntityLow->width + entityLow->width,
          .y = testEntityLow->height + entityLow->height,
      };

      struct v2 minCorner = v2_mul(diameter, -0.5f);
      struct v2 maxCorner = v2_mul(diameter, 0.5f);

      struct v2 rel = v2_sub(entityHigh->position, testEntityHigh->position);

      /* test all 4 walls and take minimum t. */
      if (WallTest(&tMin, minCorner.x, rel.x, rel.y, deltaPosition.x,
                   deltaPosition.y, minCorner.y, maxCorner.y)) {
        wallNormal = (struct v2){.x = -1, .y = 0};
        hitEntityLow = testEntityLow;
      }

      if (WallTest(&tMin, maxCorner.x, rel.x, rel.y, deltaPosition.x,
                   deltaPosition.y, minCorner.y, maxCorner.y)) {
        wallNormal = (struct v2){.x = 1, .y = 0};
        hitEntityLow = testEntityLow;
      }

      if (WallTest(&tMin, minCorner.y, rel.y, rel.x, deltaPosition.y,
                   deltaPosition.x, minCorner.x, maxCorner.x)) {
        wallNormal = (struct v2){.x = 0, .y = -1};
        hitEntityLow = testEntityLow;
      }

      if (WallTest(&tMin, maxCorner.y, rel.y, rel.x, deltaPosition.y,
                   deltaPosition.x, minCorner.x, maxCorner.x)) {
        wallNormal = (struct v2){.x = 0, .y = 1};
        hitEntityLow = testEntityLow;
      }
    }

    /* p' = tMin (1/2 a t² + v t) + p */
    v2_add_ref(&entityHigh->position, v2_mul(deltaPosition, tMin));

    /*****************************************************************
     * COLLUSION HANDLING
     *****************************************************************/
    if (hitEntityLow) {
      /*
       * add gliding to velocity
       */
      // clang-format off
      /* v' = v - vTr r */
      v2_sub_ref(&entityHigh->dPosition,
          /* vTr r */
          v2_mul(
            /* r */
            wallNormal,
            /* vTr */
            v2_dot(entityHigh->dPosition, wallNormal)
          )
      );

      /*
       *    wall
       *      |    p
       *      |  /
       *      |/
       *     /||
       *   /  ||
       * d    |▼ p'
       *      ||
       *      ||
       *      |▼ w
       *      |
       *  p  = starting point
       *  d  = desired point
       *  p' = dot product with normal clipped vector
       *  w  = not clipped vector
       *
       *  clip the delta position by gone amount
       */
      deltaPosition = v2_sub(desiredPosition, entityHigh->position);
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

      // TODO: stairs
      // entityHigh->absTileZ += hitEntityLow->dAbsTileZ;
    } else {
      break;
    }
  }

  /*****************************************************************
   * VISUAL CORRECTIONS
   *****************************************************************/
  /* use player velocity to face the direction */

  if (entityHigh->dPosition.x == 0.0f && entityHigh->dPosition.y == 0.0f)
    ;
  else if (absolute(entityHigh->dPosition.x) >
           absolute(entityHigh->dPosition.y)) {
    if (entityHigh->dPosition.x < 0)
      entityHigh->facingDirection = BITMAP_HERO_LEFT;
    else
      entityHigh->facingDirection = BITMAP_HERO_RIGHT;
  } else {
    if (entityHigh->dPosition.y > 0)
      entityHigh->facingDirection = BITMAP_HERO_BACK;
    else
      entityHigh->facingDirection = BITMAP_HERO_FRONT;
  }

  /* always write back into tile space */
  entityLow->position = WorldPositionCalculate(state->world, &state->cameraPos,
                                               entityHigh->position);
}

static u8 IsEntityHighSetValid(struct game_state *state) {
  for (u32 entityHighIndex = 1; entityHighIndex < state->entityHighCount;
       entityHighIndex++) {
    struct entity_high *entityHigh = EntityHighGet(state, entityHighIndex);
    assert(entityHigh);

    struct entity_low *entityLow = EntityLowGet(state, entityHigh->lowIndex);
    assert(entityLow);

    if (entityLow->highIndex != entityHighIndex)
      return 0;
  }

  return 1;
}

static void CameraSet(struct game_state *state,
                      struct world_position *newCameraPosition) {
  struct world *world = state->world;
  struct world_difference diff =
      WorldPositionSub(world, newCameraPosition, &state->cameraPos);
  state->cameraPos = *newCameraPosition;

  /* camera size that contains collection high frequency entities */
  const u32 tileSpanMultipler = 3;
  const u32 tileSpanX = TILES_PER_WIDTH * tileSpanMultipler;
  const u32 tileSpanY = TILES_PER_HEIGHT * tileSpanMultipler;
  struct v2 tileSpan = {(f32)tileSpanX, (f32)tileSpanY};
  v2_mul_ref(&tileSpan, world->tileSideInMeters);
  struct rectangle2 cameraBounds = RectCenterDim((struct v2){0, 0}, tileSpan);

  struct v2 entityOffsetPerFrame = v2_neg(diff.dXY);

  for (u32 entityHighIndex = 1; entityHighIndex < state->entityHighCount;) {
    struct entity_high *entityHigh = EntityHighGet(state, entityHighIndex);
    assert(entityHigh);

    /*
     *  when camera moves,
     *  negative of camera offset must be applied
     * to entites so that they are not moving with
     * camera
     * ________________ ________________
     * |              | |              |
     * |              | |              |
     * |             x| |x'            |
     * |       c      | |       c'     |
     * |              | |              |
     * |              | |              |
     * |______________| |______________|
     */
    v2_add_ref(&entityHigh->position, entityOffsetPerFrame);

    /* check if entity is in camera bounds */
    if (RectIsPointInside(cameraBounds, entityHigh->position)) {
      entityHighIndex++;
      continue;
    }

    /* transion from high to low */
    EntityMakeLowFreq(state, entityHigh->lowIndex);
  }

  assert(IsEntityHighSetValid(state));

  struct world_position minChunkPosition =
      WorldPositionCalculate(world, newCameraPosition, RectMin(&cameraBounds));
  struct world_position maxChunkPosition =
      WorldPositionCalculate(world, newCameraPosition, RectMax(&cameraBounds));
  for (u32 chunkX = minChunkPosition.chunkX; chunkX < maxChunkPosition.chunkX;
       chunkX++) {
    for (u32 chunkY = minChunkPosition.chunkY; chunkY < maxChunkPosition.chunkY;
         chunkY++) {
      struct world_chunk *chunk =
          WorldChunkGet(world, chunkX, chunkY, newCameraPosition->chunkZ, 0);
      if (!chunk)
        continue;

      for (struct world_entity_block *block = &chunk->firstBlock; block;
           block = block->next) {
        for (u32 entityIndex = 0; entityIndex < block->entityCount;
             entityIndex++) {
          u32 entityLowIndex = block->entityLowIndexes[entityIndex];
          struct entity_low *entityLow = EntityLowGet(state, entityLowIndex);
          assert(entityLow);

          if (entityLow->type == ENTITY_TYPE_INVALID)
            continue;

          if (entityLow->highIndex != 0)
            continue;

          struct world_difference diff =
              WorldPositionSub(world, &entityLow->position, newCameraPosition);
          struct v2 entityPositionRelativeToCamera = diff.dXY;
          if (!RectIsPointInside(cameraBounds, entityPositionRelativeToCamera))
            continue;

          EntityMakeHighFreq(state, entityLowIndex);
        }
      }
    }
  }

  assert(IsEntityHighSetValid(state));
}

GAMEUPDATEANDRENDER(GameUpdateAndRender) {
  struct game_state *state = memory->permanentStorage;

  /****************************************************************
   * INITIALIZATION
   ****************************************************************/
  if (!memory->initialized) {
    /* load background */
    state->bitmapBackground =
        LoadBmp(memory->PlatformReadEntireFile, "test/test_background.bmp");

    /* load shadow */
    state->bitmapShadow =
        LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_shadow.bmp");

    state->bitmapTree =
        LoadBmp(memory->PlatformReadEntireFile, "test2/tree00.bmp");

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

    /* use entity with 0 index as null */
    EntityLowAdd(state, ENTITY_TYPE_INVALID, 0);
    state->entityHighCount = 1;

    /* world creation */
    void *data = memory->permanentStorage + sizeof(*state);
    memory_arena_size_t size = memory->permanentStorageSize - sizeof(*state);
    MemoryArenaInit(&state->worldArena, data, size);

    struct world *world = MemoryArenaPush(&state->worldArena, sizeof(*world));
    state->world = world;
    WorldInit(world, 1.4f);

    /* generate procedural tile map */
    u32 screenBaseX = (I16_MAX / TILES_PER_WIDTH) / 2;
    u32 screenBaseY = (I16_MAX / TILES_PER_HEIGHT) / 2;
    u32 screenBaseZ = I16_MAX / 2;
    u32 screenX = screenBaseX;
    u32 screenY = screenBaseY;
    u32 absTileZ = screenBaseZ;

    u8 isDoorLeft = 0;
    u8 isDoorRight = 0;
    u8 isDoorTop = 0;
    u8 isDoorBottom = 0;
    u8 isDoorUp = 0;
    u8 isDoorDown = 0;

    for (u32 screenIndex = 0; screenIndex < 2000; screenIndex++) {
      u32 randomValue;

      if (1) // (isDoorUp || isDoorDown)
        randomValue = RandomNumber() % 2;
      else
        randomValue = RandomNumber() % 3;

      u8 isDoorZ = 0;
      if (randomValue == 2) {
        isDoorZ = 1;
        if (absTileZ == screenBaseZ)
          isDoorUp = 1;
        else
          isDoorDown = 1;
      } else if (randomValue == 1)
        isDoorRight = 1;
      else
        isDoorTop = 1;

      for (u32 tileY = 0; tileY < TILES_PER_HEIGHT; tileY++) {
        for (u32 tileX = 0; tileX < TILES_PER_WIDTH; tileX++) {

          u32 absTileX = screenX * TILES_PER_WIDTH + tileX;
          u32 absTileY = screenY * TILES_PER_HEIGHT + tileY;

          u32 value = TILE_WALKABLE;

          if (tileX == 0 && (!isDoorLeft || tileY != TILES_PER_HEIGHT / 2))
            value = TILE_BLOCKED;

          if (tileX == TILES_PER_WIDTH - 1 &&
              (!isDoorRight || tileY != TILES_PER_HEIGHT / 2))
            value = TILE_BLOCKED;

          if (tileY == 0 && (!isDoorBottom || tileX != TILES_PER_WIDTH / 2))
            value = TILE_BLOCKED;

          if (tileY == TILES_PER_HEIGHT - 1 &&
              (!isDoorTop || tileX != TILES_PER_WIDTH / 2))
            value = TILE_BLOCKED;

          if (tileX == 10 && tileY == 6) {
            if (isDoorUp)
              value = TILE_LADDER_UP;

            if (isDoorDown)
              value = TILE_LADDER_DOWN;
          }

          if (value & TILE_BLOCKED) {
            EntityWallAdd(state, absTileX, absTileY, absTileZ);
          }
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
        if (absTileZ == screenBaseZ)
          absTileZ = screenBaseZ + 1;
        else
          absTileZ = screenBaseZ;
      else if (randomValue == 1)
        screenX += 1;
      else
        screenY += 1;
    }

    /* set initial camera position */
    struct world_position newCameraPosition = ChunkPositionFromTilePosition(
        state->world, screenBaseX * TILES_PER_WIDTH + TILES_PER_WIDTH / 2,
        screenBaseY * TILES_PER_HEIGHT + TILES_PER_HEIGHT / 2, screenBaseZ);
    CameraSet(state, &newCameraPosition);

    memory->initialized = 1;
  }

  struct world *world = state->world;
  assert(world);

  /****************************************************************
   * CONTROLLER INPUT HANDLING
   ****************************************************************/

  for (u8 controllerIndex = 0; controllerIndex < HANDMADEHERO_CONTROLLER_COUNT;
       controllerIndex++) {
    struct game_controller_input *controller =
        GetController(input, controllerIndex);
    struct entity_low *controlledEntityLow =
        EntityLowGet(state, state->playerIndexForController[controllerIndex]);

    /* if there is no entity associated with this controller */
    if (!controlledEntityLow) {
      /* wait for start button pressed to enable */
      if (controller->start.pressed) {
        u32 entityLowIndex = EntityPlayerAdd(state);
        state->playerIndexForController[controllerIndex] = entityLowIndex;
      }
      continue;
    }

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
      struct entity_high *controlledEntityHigh =
          EntityHighGet(state, controlledEntityLow->highIndex);
      assert(controlledEntityHigh);
      controlledEntityHigh->dZ = 3.0f;
    }

    PlayerMove(state, state->playerIndexForController[controllerIndex],
               input->dtPerFrame, ddPosition);
  }

  /* sync camera with followed entity */
  struct entity_low *followedEntityLow =
      EntityLowGet(state, state->followedEntityIndex);
  if (followedEntityLow && followedEntityLow->highIndex != 0) {
    struct entity_high *followedEntityHigh =
        EntityHighGet(state, followedEntityLow->highIndex);
    assert(followedEntityHigh);

#if 0
    struct world_position newCameraPosition = state->cameraPos;
    newCameraPosition.chunkZ = followedEntityLow->position.chunkZ;

    const u32 scrollWidth = TILES_PER_WIDTH;
    const u32 scrollHeight = TILES_PER_HEIGHT;

    f32 maxDiffX = (f32)scrollWidth * 0.5f * world->tileSideInMeters;
    if (followedEntityHigh->position.x > maxDiffX)
      newCameraPosition.absTileX += scrollWidth;
    else if (followedEntityHigh->position.x < -maxDiffX)
      newCameraPosition.absTileX -= scrollWidth;

    f32 maxDiffY = (f32)scrollHeight * 0.5f * world->tileSideInMeters;
    if (followedEntityHigh->position.y > maxDiffY)
      newCameraPosition.absTileY += scrollHeight;
    else if (followedEntityHigh->position.y < -maxDiffY)
      newCameraPosition.absTileY -= scrollHeight;
#else
    struct world_position newCameraPosition = followedEntityLow->position;
#endif

    CameraSet(state, &newCameraPosition);
  }

  /****************************************************************
   * RENDERING
   ****************************************************************/
  /* unit: pixels */
  const u32 tileSideInPixels = 60;
  /* unit: pixels/meters */
  const f32 metersToPixels = (f32)tileSideInPixels / world->tileSideInMeters;

  /* drawing background */
#if 0
  DrawBitmap(&state->bitmapBackground, backbuffer, (struct v2){}, 0, 0);
#else
  DrawRectangle(backbuffer, v2(0.0f, 0.0f),
                v2((f32)backbuffer->width, (f32)backbuffer->height), 0.5f, 0.5f,
                0.5f);
#endif

  struct v2 screenCenter = {
      .x = 0.5f * (f32)backbuffer->width,
      .y = 0.5f * (f32)backbuffer->height,
  };

  /* render entities */
  for (u32 entityHighIndex = 1; entityHighIndex < state->entityHighCount;
       entityHighIndex++) {
    struct entity_high *entityHigh = EntityHighGet(state, entityHighIndex);
    assert(entityHigh);
    struct entity_low *entityLow = EntityLowGet(state, entityHigh->lowIndex);
    assert(entityLow);

    f32 playerR = 1.0f;
    f32 playerG = 1.0f;
    f32 playerB = 0.0f;

    f32 ddZ = -9.8f;
    entityHigh->z +=
        /* 1/2 a t² */
        0.5f * ddZ * square(input->dtPerFrame)
        /* + v t */
        + entityHigh->dZ * input->dtPerFrame;
    entityHigh->dZ +=
        /* a t */
        ddZ * input->dtPerFrame;
    if (entityHigh->z < 0)
      entityHigh->z = 0;
    f32 z = entityHigh->z * metersToPixels;

    f32 cAlphaShadow = 1.0f - entityHigh->z;
    if (cAlphaShadow < 0.0f)
      cAlphaShadow = 0.0f;

    struct v2 playerScreenPosition = entityHigh->position;
    /* screen's coordinate system uses y values inverse,
     * so that means going up in space means negative y values
     */
    playerScreenPosition.y *= -1;
    v2_mul_ref(&playerScreenPosition, metersToPixels);

    struct v2 playerGroundPoint = v2_add(screenCenter, playerScreenPosition);

    if (entityLow->type & ENTITY_TYPE_HERO) {
      struct bitmap_hero *bitmap =
          &state->bitmapHero[entityHigh->facingDirection];

      DrawBitmap2(&state->bitmapShadow, backbuffer, playerGroundPoint,
                  bitmap->alignX, bitmap->alignY, cAlphaShadow);
      playerGroundPoint.y -= z;

      DrawBitmap(&bitmap->torso, backbuffer, playerGroundPoint, bitmap->alignX,
                 bitmap->alignY);
      DrawBitmap(&bitmap->cape, backbuffer, playerGroundPoint, bitmap->alignX,
                 bitmap->alignY);
      DrawBitmap(&bitmap->head, backbuffer, playerGroundPoint, bitmap->alignX,
                 bitmap->alignY);
    } else {
      struct v2 playerWidthHeight = (struct v2){
          .x = entityLow->width,
          .y = entityLow->height,
      };
      v2_mul_ref(&playerWidthHeight, metersToPixels);

      struct v2 playerLeftTop =
          v2_sub(playerGroundPoint, v2_mul(playerWidthHeight, 0.5f));
      struct v2 playerRightBottom = v2_add(playerLeftTop, playerWidthHeight);

#if 1
      DrawBitmap(&state->bitmapTree, backbuffer, playerGroundPoint, 40, 80);
#else
      DrawRectangle(backbuffer, playerLeftTop, playerRightBottom,
                    /* colors */
                    playerR, playerG, playerB);
#endif
    }
  }
}
