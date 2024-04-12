#include "handmadehero/world.h"
#include <handmadehero/assert.h>
#include <handmadehero/entity.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/math.h>
#include <handmadehero/random.h>

comptime u32 TILES_PER_WIDTH = 17;
comptime u32 TILES_PER_HEIGHT = 9;

comptime struct move_spec HeroMoveSpec = {
    .unitMaxAccel = 1,
    .speed = 50.0f,
    .drag = 8.0f,
};

comptime struct move_spec FamiliarMoveSpec = {
    .unitMaxAccel = 1,
    .speed = 30.0f,
    .drag = 8.0f,
};

comptime struct move_spec SwordMoveSpec = {
    .unitMaxAccel = 0,
    .speed = 0.0f,
    .drag = 0.0f,
};

internal void
DrawRectangle(struct game_backbuffer *backbuffer, struct v2 min, struct v2 max, const struct v3 *color)
{
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

  u32 colorRGB = /* red */
      roundf32tou32(color->r * 255.0f) << 16
      /* green */
      | roundf32tou32(color->g * 255.0f) << 8
      /* blue */
      | roundf32tou32(color->b * 255.0f) << 0;

  for (i32 y = minY; y < maxY; y++) {
    u32 *pixel = (u32 *)row;
    for (i32 x = minX; x < maxX; x++) {
      *pixel = colorRGB;
      pixel++;
    }
    row += backbuffer->stride;
  }
}

internal inline void
DrawBitmap2(struct bitmap *bitmap, struct game_backbuffer *backbuffer, struct v2 pos, struct v2 align, f32 cAlpha)
{
  v2_sub_ref(&pos, align);

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

internal inline void
DrawBitmap(struct bitmap *bitmap, struct game_backbuffer *backbuffer, struct v2 pos, struct v2 align)
{
  DrawBitmap2(bitmap, backbuffer, pos, align, 1.0f);
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

internal struct bitmap
LoadBmp(pfnPlatformReadEntireFile PlatformReadEntireFile, char *filename)
{
  struct bitmap result = {0};

  struct read_file_result readResult = PlatformReadEntireFile(filename);
  if (readResult.size == 0) {
    return result;
  }

  struct bitmap_header *header = readResult.data;
  u32 *pixels = readResult.data + header->bitmapOffset;

  if (header->compression == BITMAP_COMPRESSION_BITFIELDS) {
    struct bitmap_header_compressed *cHeader = (struct bitmap_header_compressed *)header;

    i32 redShift = FindLeastSignificantBitSet((i32)cHeader->redMask);
    i32 greenShift = FindLeastSignificantBitSet((i32)cHeader->greenMask);
    i32 blueShift = FindLeastSignificantBitSet((i32)cHeader->blueMask);
    assert(redShift != greenShift);

    u32 alphaMask = ~(cHeader->redMask | cHeader->greenMask | cHeader->blueMask);
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

inline struct stored_entity *
StoredEntityGet(struct game_state *state, u32 index)
{
  struct stored_entity *result = 0;
  /* index 0 is reserved for null */
  if (index == 0)
    return result;

  if (index >= state->storedEntityCount)
    return result;

  result = state->storedEntities + index;
  return result;
}

internal inline u32
StoredEntityAdd(struct game_state *state, enum entity_type type, struct world_position *position)
{
  u32 storedEntityIndex = state->storedEntityCount;
  assert(storedEntityIndex < HANDMADEHERO_STORED_ENTITY_TOTAL);

  struct stored_entity *storedEntity = state->storedEntities + storedEntityIndex;
  assert(storedEntity);
  *storedEntity = (struct stored_entity){};
  storedEntity->sim.type = type;

  EntityChangeLocation(&state->worldArena, state->world, storedEntityIndex, storedEntity, 0, position);

  state->storedEntityCount++;

  return storedEntityIndex;
}

internal inline void
EntityHitPointsReset(struct entity *entity, u32 hitPointMax)
{
  assert(hitPointMax < ARRAY_COUNT(entity->hitPoints));
  entity->hitPointMax = hitPointMax;

  for (u32 hitPointIndex = 0; hitPointIndex < hitPointMax; hitPointIndex++) {
    struct hit_point *hitPoint = entity->hitPoints + hitPointIndex;
    hitPoint->flags = 0;
    hitPoint->filledAmount = HIT_POINT_SUB_COUNT;
  }
}

internal inline u32
CollisionRuleHash(struct game_state *state, u32 storageIndexA)
{
  /* TODO: better hash function */
  u32 hashMask = ARRAY_COUNT(state->collisionRules) - 1;
  u32 hashValue = storageIndexA & hashMask;
  return hashValue;
}

inline struct pairwise_collision_rule *
CollisionRuleGet(struct game_state *state, u32 storageIndexA)
{
  u32 hashValue = CollisionRuleHash(state, storageIndexA);
  struct pairwise_collision_rule *rule = state->collisionRules[hashValue];
  return rule;
}

internal inline void
CollisionRuleSet(struct game_state *state, u32 storageIndexA, struct pairwise_collision_rule *rule)
{
  u32 hashValue = CollisionRuleHash(state, storageIndexA);
  state->collisionRules[hashValue] = rule;
}

void
CollisionRuleAdd(struct game_state *state, u32 storageIndexA, u32 storageIndexB, u8 shouldCollide)
{
  if (storageIndexA > storageIndexB) {
    u32 temp = storageIndexA;
    storageIndexA = storageIndexB;
    storageIndexB = temp;
  }

  struct pairwise_collision_rule *found = 0;
  for (struct pairwise_collision_rule *rule = CollisionRuleGet(state, storageIndexA); rule; rule = rule->next) {
    if (rule->storageIndexA == storageIndexA && rule->storageIndexB == storageIndexB) {
      found = rule;
      break;
    }
  }

  if (!found) {
    found = state->firstFreeCollisionRule;
    if (found)
      state->firstFreeCollisionRule = found->next;
    else
      found = MemoryArenaPush(&state->worldArena, sizeof(*found));
    found->next = CollisionRuleGet(state, storageIndexA);
    CollisionRuleSet(state, storageIndexA, found);
  }

  assert(found);
  found->storageIndexA = storageIndexA;
  found->storageIndexB = storageIndexB;
  found->shouldCollide = (u8)(shouldCollide & 1);
}

internal void
CollisionRuleRemove(struct game_state *state, u32 storageIndex)
{
  // TODO: Better data structure that allows removing collision rules without searching all
  for (u32 collisionRuleIndex = 0; collisionRuleIndex < ARRAY_COUNT(state->collisionRules); collisionRuleIndex++) {
    for (struct pairwise_collision_rule **rule = &state->collisionRules[collisionRuleIndex]; *rule;) {
      if ((*rule)->storageIndexA == storageIndex || (*rule)->storageIndexB == storageIndex) {
        struct pairwise_collision_rule *removedRule = *rule;
        *rule = (*rule)->next;
        removedRule->next = state->firstFreeCollisionRule;
        state->firstFreeCollisionRule = removedRule;
      } else {
        rule = &(*rule)->next;
      }
    }
  }
}

internal inline u32
SwordAdd(struct game_state *state)
{
  u32 storedEntityIndex = StoredEntityAdd(state, ENTITY_TYPE_SWORD, 0);
  struct stored_entity *stored = StoredEntityGet(state, storedEntityIndex);
  assert(stored);
  struct entity *entity = &stored->sim;

  entity->dim.x = state->world->tileSideInMeters;
  entity->dim.y = state->world->tileSideInMeters;

  return storedEntityIndex;
}

internal inline u32
HeroAdd(struct game_state *state)
{
  struct world_position *entityPosition = &state->cameraPosition;
  u32 storedEntityIndex = StoredEntityAdd(state, ENTITY_TYPE_HERO, entityPosition);
  struct stored_entity *stored = StoredEntityGet(state, storedEntityIndex);
  assert(stored);
  struct entity *entity = &stored->sim;

  entity->dim.x = 0.5f;
  entity->dim.y = 1.0f;
  EntityAddFlag(entity, ENTITY_FLAG_COLLIDE);
  EntityHitPointsReset(entity, 3);

  u32 swordIndex = SwordAdd(state);
  entity->sword.index = swordIndex;
  CollisionRuleAdd(state, storedEntityIndex, swordIndex, 0);

  /* if followed entity, does not exits */
  if (state->followedEntityIndex == 0)
    state->followedEntityIndex = storedEntityIndex;

  return storedEntityIndex;
}

internal inline u32
MonsterAdd(struct game_state *state, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position entityPosition = ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  u32 storedEntityIndex = StoredEntityAdd(state, ENTITY_TYPE_MONSTER, &entityPosition);
  struct stored_entity *stored = StoredEntityGet(state, storedEntityIndex);
  assert(stored);
  struct entity *entity = &stored->sim;

  entity->dim.x = 0.5f;
  entity->dim.y = 1.0f;
  EntityAddFlag(entity, ENTITY_FLAG_COLLIDE);

  EntityHitPointsReset(entity, 3);

  return storedEntityIndex;
}

internal inline u32
FamiliarAdd(struct game_state *state, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position entityPosition = ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  u32 storedEntityIndex = StoredEntityAdd(state, ENTITY_TYPE_FAMILIAR, &entityPosition);
  struct stored_entity *stored = StoredEntityGet(state, storedEntityIndex);
  assert(stored);
  struct entity *entity = &stored->sim;

  entity->dim.x = 0.5f;
  entity->dim.y = 1.0f;
  entity->flags = 0;

  return storedEntityIndex;
}

internal inline u32
WallAdd(struct game_state *state, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position entityPosition = ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  u32 storedEntityIndex = StoredEntityAdd(state, ENTITY_TYPE_WALL, &entityPosition);
  struct stored_entity *stored = StoredEntityGet(state, storedEntityIndex);
  assert(stored);
  struct entity *entity = &stored->sim;

  entity->dim.x = state->world->tileSideInMeters;
  entity->dim.y = state->world->tileSideInMeters;
  EntityAddFlag(entity, ENTITY_FLAG_COLLIDE);

  return storedEntityIndex;
}

internal inline u32
StairAdd(struct game_state *state, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position entityPosition = ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  u32 storedEntityIndex = StoredEntityAdd(state, ENTITY_TYPE_STAIRWELL, &entityPosition);
  struct stored_entity *stored = StoredEntityGet(state, storedEntityIndex);
  assert(stored);
  struct entity *entity = &stored->sim;

  entity->dim.x = state->world->tileSideInMeters;
  entity->dim.y = state->world->tileSideInMeters;
  entity->dim.z = state->world->tileDepthInMeters;

  return storedEntityIndex;
}

internal inline void
DrawHitPoints(struct game_backbuffer *backbuffer, struct entity *entity, struct v2 *entityGroundPoint,
              f32 metersToPixels)
{
  if (entity->hitPointMax == 0)
    return;

  struct v2 healthDim = v2(0.2f, 0.2f);
  f32 spacingX = 1.5f * healthDim.x;
  struct v2 hitPosition = v2(-0.5f * (f32)(entity->hitPointMax - 1) * spacingX, 0.25f);
  v2_sub_ref(&hitPosition, v2(healthDim.x * 0.5f, 0));
  struct v2 dHitPosition = v2(spacingX, 0);

  v2_mul_ref(&healthDim, metersToPixels);
  v2_mul_ref(&hitPosition, metersToPixels);
  v2_mul_ref(&dHitPosition, metersToPixels);

  for (u32 healthIndex = 0; healthIndex < entity->hitPointMax; healthIndex++) {
    struct hit_point *hitPoint = entity->hitPoints + healthIndex;

    struct v2 min = v2_add(*entityGroundPoint, hitPosition);
    struct v2 max = v2_add(min, healthDim);

    struct v3 color = v3(1.0f, 0.0f, 0.0f);
    if (hitPoint->filledAmount == 0) {
      color = v3(0.2f, 0.2f, 0.2f);
    }

    DrawRectangle(backbuffer, min, max, &color);

    v2_add_ref(&hitPosition, dHitPosition);
  }
}

void
GameUpdateAndRender(struct game_memory *memory, struct game_input *input, struct game_backbuffer *backbuffer)
{
  struct game_state *state = memory->permanentStorage;
  f32 dt = input->dtPerFrame;

  /****************************************************************
   * INITIALIZATION
   ****************************************************************/
  if (!memory->initialized) {
    /* load background */
    state->bitmapBackground = LoadBmp(memory->PlatformReadEntireFile, "test/test_background.bmp");

    /* load shadow */
    state->bitmapShadow = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_shadow.bmp");

    state->bitmapTree = LoadBmp(memory->PlatformReadEntireFile, "test2/tree00.bmp");

    state->bitmapSword = LoadBmp(memory->PlatformReadEntireFile, "test2/rock03.bmp");

    state->bitmapStairwell = LoadBmp(memory->PlatformReadEntireFile, "test2/rock02.bmp");

    /* load hero bitmaps */
    struct bitmap_hero *bitmapHero = &state->bitmapHero[BITMAP_HERO_FRONT];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_front_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_front_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_front_cape.bmp");
    bitmapHero->align = v2(72, 182);

    bitmapHero = &state->bitmapHero[BITMAP_HERO_BACK];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_back_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_back_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_back_cape.bmp");
    bitmapHero->align = v2(72, 182);

    bitmapHero = &state->bitmapHero[BITMAP_HERO_LEFT];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_left_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_left_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_left_cape.bmp");
    bitmapHero->align = v2(72, 182);

    bitmapHero = &state->bitmapHero[BITMAP_HERO_RIGHT];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_right_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_right_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_right_cape.bmp");
    bitmapHero->align = v2(72, 182);

    /* use entity with 0 index as null */
    StoredEntityAdd(state, ENTITY_TYPE_INVALID, 0);

    /* world creation */
    void *data = memory->permanentStorage + sizeof(*state);
    memory_arena_size_t size = memory->permanentStorageSize - sizeof(*state);
    MemoryArenaInit(&state->worldArena, data, size);

    struct world *world = MemoryArenaPush(&state->worldArena, sizeof(*world));
    state->world = world;
    WorldInit(world, 1.4f);

    /* generate procedural tile map */
    u32 screenBaseX = (U32_MAX / TILES_PER_WIDTH) / 2;
    u32 screenBaseY = (U32_MAX / TILES_PER_HEIGHT) / 2;
    u32 screenBaseZ = U32_MAX / 2;
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

      if (isDoorUp || isDoorDown)
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

          u8 shouldBlock = 0;

          if (tileX == 0 && (!isDoorLeft || tileY != TILES_PER_HEIGHT / 2))
            shouldBlock = 1;

          if (tileX == TILES_PER_WIDTH - 1 && (!isDoorRight || tileY != TILES_PER_HEIGHT / 2))
            shouldBlock = 1;

          if (tileY == 0 && (!isDoorBottom || tileX != TILES_PER_WIDTH / 2))
            shouldBlock = 1;

          if (tileY == TILES_PER_HEIGHT - 1 && (!isDoorTop || tileX != TILES_PER_WIDTH / 2))
            shouldBlock = 1;

          if (shouldBlock) {
            WallAdd(state, absTileX, absTileY, absTileZ);
          } else if (isDoorZ) {
            if (tileX == 10 && tileY == 6) {
              StairAdd(state, absTileX, absTileY, isDoorDown ? absTileZ - 1 : absTileZ);
            }
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
    u32 initialCameraX = screenBaseX * TILES_PER_WIDTH + TILES_PER_WIDTH / 2;
    u32 initialCameraY = screenBaseY * TILES_PER_HEIGHT + TILES_PER_HEIGHT / 2;
    u32 initialCameraZ = screenBaseZ;
    MonsterAdd(state, initialCameraX - 4, initialCameraY + 2, initialCameraZ);
    MonsterAdd(state, initialCameraX - 2, initialCameraY + 2, initialCameraZ);
    FamiliarAdd(state, initialCameraX - 2, initialCameraY + 2, initialCameraZ);

    struct world_position initialCameraPosition =
        ChunkPositionFromTilePosition(state->world, initialCameraX, initialCameraY, initialCameraZ);
    state->cameraPosition = initialCameraPosition;

    memory->initialized = 1;
  }

  struct world *world = state->world;
  assert(world);

  /****************************************************************
   * CONTROLLER INPUT HANDLING
   ****************************************************************/

  for (u8 controllerIndex = 0; controllerIndex < HANDMADEHERO_CONTROLLER_COUNT; controllerIndex++) {
    struct game_controller_input *controller = GetController(input, controllerIndex);
    struct controlled_hero *conHero = state->controlledHeroes + controllerIndex;

    /* if there is no entity associated with this controller */
    if (conHero->entityIndex == 0) {
      /* wait for start button pressed to enable */
      if (controller->start.pressed) {
        *conHero = (struct controlled_hero){};
        conHero->entityIndex = HeroAdd(state);
      }
      continue;
    }

    /* acceleration */
    conHero->ddPosition = (struct v3){};

    /* analog controller */
    if (controller->isAnalog) {
      conHero->ddPosition = v3(controller->stickAverageX, controller->stickAverageY, 0);
    }

    /* digital controller */
    else {

      if (controller->moveLeft.pressed) {
        conHero->ddPosition.x = -1.0f;
      }

      if (controller->moveRight.pressed) {
        conHero->ddPosition.x = 1.0f;
      }

      if (controller->moveDown.pressed) {
        conHero->ddPosition.y = -1.0f;
      }

      if (controller->moveUp.pressed) {
        conHero->ddPosition.y = 1.0f;
      }
    }

    conHero->dZ = 0.0f;
    if (controller->start.pressed) {
      conHero->dZ = 3.0f;
    }

    conHero->dSword = (struct v2){};
    if (controller->actionUp.pressed) {
      conHero->dSword = v2(0.0f, 1.0f);
    } else if (controller->actionDown.pressed) {
      conHero->dSword = v2(0.0f, -1.0f);
    } else if (controller->actionLeft.pressed) {
      conHero->dSword = v2(-1.0f, 0.0f);
    } else if (controller->actionRight.pressed) {
      conHero->dSword = v2(1.0f, 0.0f);
    }
  }

  comptime u32 tileSpanMultipler = 3;
  comptime u32 tileSpanX = TILES_PER_WIDTH * tileSpanMultipler;
  comptime u32 tileSpanY = TILES_PER_HEIGHT * tileSpanMultipler;
  comptime u32 tileSpanZ = 1;
  struct v3 tileSpan = {(f32)tileSpanX, (f32)tileSpanY, (f32)tileSpanZ};
  v3_mul_ref(&tileSpan, world->tileSideInMeters);
  struct rect cameraBounds = RectCenterDim(v3(0.0f, 0.0f, 0.0f), tileSpan);

  struct memory_arena simArena;
  MemoryArenaInit(&simArena, memory->transientStorage, memory->transientStorageSize);
  struct sim_region *simRegion =
      BeginSimRegion(&simArena, state, state->world, state->cameraPosition, cameraBounds, dt);

  /****************************************************************
   * RENDERING
   ****************************************************************/
  /* unit: pixels */
  const u32 tileSideInPixels = 60;
  /* unit: pixels/meters */
  const f32 metersToPixels = (f32)tileSideInPixels / world->tileSideInMeters;

/* drawing background */
#if 0
  DrawBitmap(&state->bitmapBackground, backbuffer, v2(0.0f, 0.0f), v2(0, 0));
#else
  struct v3 backgroundColor = v3(0.5f, 0.5f, 0.5f);
  DrawRectangle(backbuffer, v2(0.0f, 0.0f), v2((f32)backbuffer->width, (f32)backbuffer->height), &backgroundColor);
#endif

  struct v2 screenCenter = {
      .x = 0.5f * (f32)backbuffer->width,
      .y = 0.5f * (f32)backbuffer->height,
  };

  /* render entities */
  for (u32 entityIndex = 0; entityIndex < simRegion->entityCount; entityIndex++) {
    struct entity *entity = simRegion->entities + entityIndex;
    assert(entity);

    if (entity->type == ENTITY_TYPE_INVALID)
      continue;

    if (EntityIsFlagSet(entity, ENTITY_FLAG_NONSPACIAL))
      continue;

    if (!entity->updatable)
      continue;

    struct v2 entityScreenPosition = entity->position.xy;
    /* screen's coordinate system uses y values inverse,
     * so that means going up in space means negative y values
     */
    entityScreenPosition.y *= -1;
    v2_mul_ref(&entityScreenPosition, metersToPixels);

    struct v2 entityGroundPoint = v2_add(screenCenter, entityScreenPosition);

    if (entity->type & ENTITY_TYPE_HERO) {
      /* update */
      for (u8 controllerIndex = 0; controllerIndex < HANDMADEHERO_CONTROLLER_COUNT; controllerIndex++) {
        struct controlled_hero *conHero = state->controlledHeroes + controllerIndex;
        if (conHero->entityIndex != entity->storageIndex)
          continue;

        /* jump */
        if (conHero->dZ != 0.0f)
          entity->dPosition.z = conHero->dZ;

        /* move */
        EntityMove(state, simRegion, entity, dt, &HeroMoveSpec, conHero->ddPosition);

        /* sword */
        struct v2 dSword = conHero->dSword;
        if (dSword.x != 0.0f || dSword.y != 0.0f) {
          struct entity *sword = entity->sword.ptr;
          assert(sword);
          struct stored_entity *storedSword = StoredEntityGet(state, sword->storageIndex);

          if (!WorldPositionIsValid(&storedSword->position)) {
            EntityClearFlag(sword, ENTITY_FLAG_NONSPACIAL);
            sword->position = entity->position;
            sword->distanceRemaining = 5.0f;
            sword->dPosition.xy = v2_mul(dSword, 5.0f);
          }
        }
      }

      /* render */
      struct bitmap_hero *bitmap = &state->bitmapHero[entity->facingDirection];
      f32 cAlphaShadow = 1.0f - entity->position.z;
      if (cAlphaShadow < 0.0f)
        cAlphaShadow = 0.0f;

      DrawHitPoints(backbuffer, entity, &entityGroundPoint, metersToPixels);

      DrawBitmap2(&state->bitmapShadow, backbuffer, entityGroundPoint, bitmap->align, cAlphaShadow);
      entityGroundPoint.y -= entity->position.z * metersToPixels;

      DrawBitmap(&bitmap->torso, backbuffer, entityGroundPoint, bitmap->align);
      DrawBitmap(&bitmap->cape, backbuffer, entityGroundPoint, bitmap->align);
      DrawBitmap(&bitmap->head, backbuffer, entityGroundPoint, bitmap->align);
    }

    else if (entity->type & ENTITY_TYPE_FAMILIAR) {
      /* update */
      struct entity *familiar = entity;
      struct entity *closestHero = 0;
      /* 10m maximum search radius */
      f32 closestHeroDistanceSq = square(10.0f);

      for (u32 testEntityIndex = 0; testEntityIndex < simRegion->entityCount; testEntityIndex++) {
        struct entity *testEntity = simRegion->entities + testEntityIndex;

        if (testEntity->type == ENTITY_TYPE_INVALID || !(testEntity->type & ENTITY_TYPE_HERO))
          continue;

        struct entity *heroEntity = testEntity;

        f32 testDistanceSq = v3_length_square(v3_sub(familiar->position, heroEntity->position));
        if (testDistanceSq < closestHeroDistanceSq) {
          closestHero = heroEntity;
          closestHeroDistanceSq = testDistanceSq;
        }
      }

      struct v3 ddPosition = {};
      if (closestHero && closestHeroDistanceSq > square(3.0f)) {
        /* there is hero nearby, follow him */
        f32 oneOverLength = 1.0f / SquareRoot(closestHeroDistanceSq);
        ddPosition = v3_mul(v3_sub(closestHero->position, familiar->position), oneOverLength);
      }

      EntityMove(state, simRegion, entity, dt, &FamiliarMoveSpec, ddPosition);

      entity->tBob += dt;
      if (entity->tBob > 2.0f * PI32)
        entity->tBob -= 2.0f * PI32;

      /* render */
      f32 bobSin = Sin(2.0f * entity->tBob);
      struct bitmap_hero *bitmap = &state->bitmapHero[entity->facingDirection];

      f32 cAlphaShadow = (0.5f * 1.0f) - (0.2f * bobSin);

      DrawBitmap2(&state->bitmapShadow, backbuffer, entityGroundPoint, bitmap->align, cAlphaShadow);
      entityGroundPoint.y -= 15 * bobSin;

      DrawBitmap(&bitmap->head, backbuffer, entityGroundPoint, bitmap->align);
    }

    else if (entity->type & ENTITY_TYPE_MONSTER) {
      /* render */
      struct bitmap_hero *bitmap = &state->bitmapHero[entity->facingDirection];
      f32 cAlphaShadow = 1.0f;

      DrawHitPoints(backbuffer, entity, &entityGroundPoint, metersToPixels);
      DrawBitmap2(&state->bitmapShadow, backbuffer, entityGroundPoint, bitmap->align, cAlphaShadow);

      DrawBitmap(&bitmap->torso, backbuffer, entityGroundPoint, bitmap->align);
    }

    else if (entity->type & ENTITY_TYPE_SWORD) {
      /* update */
      struct entity *sword = entity;
      struct v3 oldPosition = sword->position;
      EntityMove(state, simRegion, entity, dt, &SwordMoveSpec, v3(0, 0, 0));

      f32 distanceTraveled = v3_length(v3_sub(sword->position, oldPosition));
      sword->distanceRemaining -= distanceTraveled;
      if (sword->distanceRemaining < 0.0f) {
        EntityAddFlag(sword, ENTITY_FLAG_NONSPACIAL);
        CollisionRuleRemove(state, sword->storageIndex);
      }

      /* render */
      f32 cAlphaShadow = 1.0f;
      DrawBitmap2(&state->bitmapShadow, backbuffer, entityGroundPoint, v2(72, 182), cAlphaShadow);
      DrawBitmap(&state->bitmapSword, backbuffer, entityGroundPoint, v2(29, 10));
    }

    else if (entity->type & ENTITY_TYPE_WALL) {
#if 1
      DrawBitmap(&state->bitmapTree, backbuffer, entityGroundPoint, v2(40, 80));
#else
      comptime struct v3 color = {1.0f, 1.0f, 0.0f};

      struct v2 entityWidthHeight = entity->dim.xy;
      v2_mul_ref(&entityWidthHeight, metersToPixels);

      struct v2 entityLeftTop = v2_sub(entityGroundPoint, v2_mul(entityWidthHeight, 0.5f));
      struct v2 entityRightBottom = v2_add(entityLeftTop, entityWidthHeight);
      DrawRectangle(backbuffer, entityLeftTop, entityRightBottom, &color);
#endif
    }

    else if (entity->type & ENTITY_TYPE_STAIRWELL) {
#if 1
      DrawBitmap(&state->bitmapStairwell, backbuffer, entityGroundPoint, v2(37, 37));
#else
      comptime struct v3 color = {1.0f, 1.0f, 0.0f};

      struct v2 entityWidthHeight = entity->dim.xy;
      v2_mul_ref(&entityWidthHeight, metersToPixels);

      struct v2 entityLeftTop = v2_sub(entityGroundPoint, v2_mul(entityWidthHeight, 0.5f));
      struct v2 entityRightBottom = v2_add(entityLeftTop, entityWidthHeight);
      DrawRectangle(backbuffer, entityLeftTop, entityRightBottom, &color);
#endif
    }

    else {
      InvalidCodePath;
    }
  }

  EndSimRegion(simRegion, state);
}
