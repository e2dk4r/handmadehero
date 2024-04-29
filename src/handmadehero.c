#include <handmadehero/assert.h>
#include <handmadehero/entity.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/math.h>
#include <handmadehero/random.h>
#include <handmadehero/render_group.h>
#include <handmadehero/world.h>

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
DrawRectangle(struct bitmap *buffer, struct v2 min, struct v2 max, const struct v4 *color)
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

  if (maxX > (i32)buffer->width)
    maxX = (i32)buffer->width;

  if (maxY > (i32)buffer->height)
    maxY = (i32)buffer->height;

  u8 *row = buffer->memory
            /* x offset */
            + (minX * BITMAP_BYTES_PER_PIXEL)
            /* y offset */
            + (minY * buffer->stride);

  u32 colorRGBA =
      /* alpha */
      roundf32tou32(color->a * 255.0f) << 24
      /* red */
      | roundf32tou32(color->r * 255.0f) << 16
      /* green */
      | roundf32tou32(color->g * 255.0f) << 8
      /* blue */
      | roundf32tou32(color->b * 255.0f) << 0;

  for (i32 y = minY; y < maxY; y++) {
    u32 *pixel = (u32 *)row;
    for (i32 x = minX; x < maxX; x++) {
      *pixel = colorRGBA;
      pixel++;
    }
    row += buffer->stride;
  }
}

internal inline void
DrawRectangleOutline(struct bitmap *buffer, struct v2 min, struct v2 max, const struct v4 *color, f32 thickness)
{
  // NOTE(e2dk4r): top and bottom
  DrawRectangle(buffer, v2(min.x - thickness, min.y - thickness), v2(max.x + thickness, min.y + thickness), color);
  DrawRectangle(buffer, v2(min.x - thickness, max.y - thickness), v2(max.x + thickness, max.y + thickness), color);

  // NOTE(e2dk4r): left right
  DrawRectangle(buffer, v2(min.x - thickness, min.y - thickness), v2(min.x + thickness, max.y + thickness), color);
  DrawRectangle(buffer, v2(max.x - thickness, min.y - thickness), v2(max.x + thickness, max.y + thickness), color);
}

internal inline void
DrawBitmap2(struct bitmap *buffer, struct bitmap *bitmap, struct v2 pos, struct v2 align, f32 cAlpha)
{
  v2_sub_ref(&pos, align);

  i32 minX = roundf32toi32(pos.x);
  i32 minY = roundf32toi32(pos.y);
  i32 maxX = roundf32toi32(pos.x + (f32)bitmap->width);
  i32 maxY = roundf32toi32(pos.y + (f32)bitmap->height);

  i32 srcOffsetX = 0;
  if (minX < 0) {
    srcOffsetX = -minX;
    minX = 0;
  }

  i32 srcOffsetY = 0;
  if (minY < 0) {
    srcOffsetY = -minY;
    minY = 0;
  }

  if (maxX > (i32)buffer->width)
    maxX = (i32)buffer->width;

  if (maxY > (i32)buffer->height)
    maxY = (i32)buffer->height;

  /* bitmap file pixels goes bottom to up */
  u8 *srcRow = (u8 *)bitmap->memory
               /* last row offset */
               + srcOffsetY * bitmap->stride
               /* clipped offset */
               + srcOffsetX * BITMAP_BYTES_PER_PIXEL;
  u8 *dstRow = (u8 *)buffer->memory
               /* x offset */
               + minX * BITMAP_BYTES_PER_PIXEL
               /* y offset */
               + minY * buffer->stride;
  for (i32 y = minY; y < maxY; y++) {
    u32 *src = (u32 *)srcRow;
    u32 *dst = (u32 *)dstRow;

    for (i32 x = minX; x < maxX; x++) {
      // source channels
      f32 sA = (f32)((*src >> 24) & 0xff);
      f32 sR = cAlpha * (f32)((*src >> 16) & 0xff);
      f32 sG = cAlpha * (f32)((*src >> 8) & 0xff);
      f32 sB = cAlpha * (f32)((*src >> 0) & 0xff);

      // normalized sA
      f32 nsA = (sA / 255.0f) * cAlpha;

      // destination channels
      f32 dA = (f32)((*dst >> 24) & 0xff);
      f32 dR = (f32)((*dst >> 16) & 0xff);
      f32 dG = (f32)((*dst >> 8) & 0xff);
      f32 dB = (f32)((*dst >> 0) & 0xff);

      // normalized dA
      f32 ndA = (dA / 255.0f);

      // percentage of normalized sA to be applied
      f32 psA = (1.0f - nsA);

      /*
       * Math of calculating blended alpha
       * videoId:   bidrZj1YosA
       * timestamp: 01:06:19
       */
      f32 a = 255.0f * (nsA + ndA - nsA * ndA);
      f32 r = psA * dR + sR;
      f32 g = psA * dG + sG;
      f32 b = psA * dB + sB;

      *dst =
          /* alpha */
          (u32)(a + 0.5f) << 24
          /* red */
          | (u32)(r + 0.5f) << 16
          /* green */
          | (u32)(g + 0.5f) << 8
          /* blue */
          | (u32)(b + 0.5f) << 0;

      dst++;
      src++;
    }

    dstRow += buffer->stride;
    srcRow += bitmap->stride;
  }
}

internal inline void
DrawBitmap(struct bitmap *buffer, struct bitmap *bitmap, struct v2 pos, struct v2 align)
{
  DrawBitmap2(buffer, bitmap, pos, align, 1.0f);
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
  u8 *pixels = readResult.data + header->bitmapOffset;

  if (header->compression == BITMAP_COMPRESSION_BITFIELDS) {
    struct bitmap_header_compressed *cHeader = (struct bitmap_header_compressed *)header;

    i32 redShift = FindLeastSignificantBitSet((i32)cHeader->redMask);
    i32 greenShift = FindLeastSignificantBitSet((i32)cHeader->greenMask);
    i32 blueShift = FindLeastSignificantBitSet((i32)cHeader->blueMask);
    assert(redShift != greenShift);

    u32 alphaMask = ~(cHeader->redMask | cHeader->greenMask | cHeader->blueMask);
    i32 alphaShift = FindLeastSignificantBitSet((i32)alphaMask);

    u32 *srcDest = (u32 *)pixels;
    for (i32 y = 0; y < header->height; y++) {
      for (i32 x = 0; x < header->width; x++) {

        u32 value = *srcDest;

        // extract pixel from file
        f32 a = (f32)((value >> alphaShift) & 0xff);
        f32 r = (f32)((value >> redShift) & 0xff);
        f32 g = (f32)((value >> greenShift) & 0xff);
        f32 b = (f32)((value >> blueShift) & 0xff);

        /*
         * Store channels values pre-multiplied with alpha.
         */
        f32 nA = a / 255.0f;
        r *= nA;
        g *= nA;
        b *= nA;

        *srcDest =
            /* alpha */
            (u32)(a + 0.5f) << 24
            /* red */
            | (u32)(r + 0.5f) << 16
            /* green */
            | (u32)(g + 0.5f) << 8
            /* blue */
            | (u32)(b + 0.5f) << 0;

        srcDest++;
      }
    }
  }

  result.width = (u32)header->width;
  if (header->width < 0)
    result.width = (u32)-header->width;

  result.height = (u32)header->height;
  if (header->height < 0)
    result.height = (u32)-header->height;

  result.stride = -(i32)result.width * BITMAP_BYTES_PER_PIXEL;
  result.memory = pixels - (i32)(result.height - 1) * result.stride;

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

struct stored_entity_add_result {
  u32 index;
  struct stored_entity *stored;
};

internal inline struct stored_entity_add_result
StoredEntityAdd(struct game_state *state, enum entity_type type, struct world_position *position,
                struct entity_collision_volume_group *collision)
{
  u32 storedEntityIndex = state->storedEntityCount;
  assert(storedEntityIndex < ARRAY_COUNT(state->storedEntities));

  struct stored_entity *stored = state->storedEntities + storedEntityIndex;
  assert(stored);
  *stored = (struct stored_entity){};
  struct entity *entity = &stored->sim;
  entity->type = type;
  entity->collision = collision;

  EntityChangeLocation(&state->worldArena, state->world, storedEntityIndex, stored, 0, position);

  state->storedEntityCount++;

  return (struct stored_entity_add_result){
      .index = storedEntityIndex,
      .stored = stored,
  };
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

internal struct entity_collision_volume_group *
MakeSimpleGroundedCollision(struct memory_arena *arena, struct v3 dim)
{
  struct entity_collision_volume_group *result = MemoryArenaPush(arena, sizeof(*result));
  ZeroMemory(result, sizeof(*result));

  result->volumeCount = 1;

  result->totalVolume.offset = v3(0, 0, 0.5f * dim.z);
  result->totalVolume.dim = dim;

  result->volumes = &result->totalVolume;

  return result;
}

internal inline struct world_position
ChunkPositionFromTilePosition(struct world *world, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  f32 tileSideInMeters = 1.4f;
  f32 tileDepthInMeters = 3.0f;

  struct world_position basePosition = {};
  struct v3 tileDim = {tileSideInMeters, tileSideInMeters, tileDepthInMeters};
  struct world_position result = WorldPositionCalculate(
      world, &basePosition, v3_hadamard(tileDim, v3((f32)absTileX, (f32)absTileY, (f32)absTileZ)));

  assert(IsWorldPositionOffsetCalculated(world, &result.offset));
  return result;
}

internal inline u32
SwordAdd(struct game_state *state)
{
  struct stored_entity_add_result addResult = StoredEntityAdd(state, ENTITY_TYPE_SWORD, 0, state->swordCollision);
  struct stored_entity *stored = addResult.stored;
  struct entity *entity = &stored->sim;
  EntityAddFlag(entity, ENTITY_FLAG_COLLIDE);

  return addResult.index;
}

internal inline u32
HeroAdd(struct game_state *state)
{
  struct world_position *entityPosition = &state->cameraPosition;
  struct stored_entity_add_result addResult =
      StoredEntityAdd(state, ENTITY_TYPE_HERO, entityPosition, state->heroCollision);

  struct stored_entity *stored = addResult.stored;
  struct entity *entity = &stored->sim;

  EntityAddFlag(entity, ENTITY_FLAG_COLLIDE);
  EntityHitPointsReset(entity, 3);

  u32 swordIndex = SwordAdd(state);
  entity->sword.index = swordIndex;
  CollisionRuleAdd(state, addResult.index, swordIndex, 0);

  /* if followed entity, does not exits */
  if (state->followedEntityIndex == 0)
    state->followedEntityIndex = addResult.index;

  return addResult.index;
}

internal inline u32
MonsterAdd(struct game_state *state, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position entityPosition = ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  struct stored_entity_add_result addResult =
      StoredEntityAdd(state, ENTITY_TYPE_MONSTER, &entityPosition, state->monsterCollision);
  struct stored_entity *stored = addResult.stored;
  struct entity *entity = &stored->sim;

  EntityAddFlag(entity, ENTITY_FLAG_COLLIDE);

  EntityHitPointsReset(entity, 3);

  return addResult.index;
}

internal inline u32
FamiliarAdd(struct game_state *state, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position entityPosition = ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  struct stored_entity_add_result addResult =
      StoredEntityAdd(state, ENTITY_TYPE_FAMILIAR, &entityPosition, state->familiarCollision);
  struct stored_entity *stored = addResult.stored;
  struct entity *entity = &stored->sim;

  entity->flags = 0;

  return addResult.index;
}

internal inline u32
WallAdd(struct game_state *state, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position entityPosition = ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  struct stored_entity_add_result addResult =
      StoredEntityAdd(state, ENTITY_TYPE_WALL, &entityPosition, state->wallCollision);
  struct stored_entity *stored = addResult.stored;
  struct entity *entity = &stored->sim;

  EntityAddFlag(entity, ENTITY_FLAG_COLLIDE);

  return addResult.index;
}

internal inline u32
StairAdd(struct game_state *state, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position entityPosition = ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  struct stored_entity_add_result addResult =
      StoredEntityAdd(state, ENTITY_TYPE_STAIRWELL, &entityPosition, state->stairwellCollision);
  struct stored_entity *stored = addResult.stored;
  struct entity *entity = &stored->sim;

  entity->walkableDim = state->stairwellCollision->totalVolume.dim;
  entity->walkableHeight = state->floorHeight;
  EntityAddFlag(entity, ENTITY_FLAG_COLLIDE);

  return addResult.index;
}

internal inline void
SpaceAdd(struct game_state *state, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position entityPosition = ChunkPositionFromTilePosition(state->world, absTileX, absTileY, absTileZ);
  struct stored_entity_add_result addResult =
      StoredEntityAdd(state, ENTITY_TYPE_SPACE, &entityPosition, state->roomCollision);
  struct stored_entity *stored = addResult.stored;
  struct entity *entity = &stored->sim;
  EntityAddFlag(entity, ENTITY_FLAG_TRAVERSABLE);
}

internal inline void
PushHitPoints(struct render_group *group, struct entity *entity)
{
  if (entity->hitPointMax == 0)
    return;

  struct v2 healthDim = v2(0.2f, 0.2f);
  f32 spacingX = 1.5f * healthDim.x;
  struct v2 hitPosition = v2(-0.5f * (f32)(entity->hitPointMax - 1) * spacingX, -0.25f);
  struct v2 dHitPosition = v2(spacingX, 0);

  for (u32 healthIndex = 0; healthIndex < entity->hitPointMax; healthIndex++) {
    struct hit_point *hitPoint = entity->hitPoints + healthIndex;

    struct v4 color = v4(1.0f, 0.0f, 0.0f, 1.0f);
    if (hitPoint->filledAmount == 0) {
      color = v4(0.2f, 0.2f, 0.2f, 1.0f);
    }

    PushRect(group, hitPosition, 0.0f, healthDim, color);

    v2_add_ref(&hitPosition, dHitPosition);
  }
}

internal struct bitmap
MakeEmptyBitmap(struct memory_arena *arena, u32 width, u32 height)
{
  u32 totalBitmapSize = width * height * BITMAP_BYTES_PER_PIXEL;
  struct bitmap bitmap = {
      .width = width,
      .height = height,
      .stride = (i32)width * BITMAP_BYTES_PER_PIXEL,
      .memory = MemoryArenaPush(arena, totalBitmapSize),
  };

  return bitmap;
}

internal void
ClearBitmap(struct bitmap *bitmap)
{
  if (bitmap->memory) {
    u32 totalBitmapSize = bitmap->width * bitmap->height * BITMAP_BYTES_PER_PIXEL;
    ZeroMemory(bitmap->memory, totalBitmapSize);
  }
}

internal inline u8
IsGroundBufferEmpty(struct ground_buffer *groundBuffer)
{
  return !WorldPositionIsValid(&groundBuffer->position);
}

internal void
FillGroundChunk(struct transient_state *transientState, struct game_state *state, struct ground_buffer *groundBuffer,
                struct world_position *chunkPosition)
{
  struct bitmap *buffer = &groundBuffer->bitmap;

  groundBuffer->position = *chunkPosition;

  f32 width = (f32)buffer->width;
  f32 height = (f32)buffer->width;

  for (i32 chunkOffsetY = -1; chunkOffsetY <= 1; chunkOffsetY++) {
    for (i32 chunkOffsetX = -1; chunkOffsetX <= 1; chunkOffsetX++) {
      u32 chunkX = chunkPosition->chunkX + (u32)chunkOffsetX;
      u32 chunkY = chunkPosition->chunkY + (u32)chunkOffsetY;
      u32 chunkZ = chunkPosition->chunkZ;

      u32 seed = 139 * chunkX + 593 * chunkY + 329 * chunkZ;
      struct random_series series = RandomSeed(seed);

      struct v2 center = v2((f32)chunkOffsetX * width, (f32)chunkOffsetY * width);
      center.y = -center.y; /* y flipped for draw */

      for (u32 grassIndex = 0; grassIndex < 100; grassIndex++) {
        struct bitmap *stamp = 0;
        if (RandomChoice(&series, 2))
          stamp = state->textureGrass + RandomChoice(&series, ARRAY_COUNT(state->textureGrass));
        else
          stamp = state->textureGround + RandomChoice(&series, ARRAY_COUNT(state->textureGround));

        struct v2 stampCenter = v2_mul(v2u(stamp->width, stamp->height), 0.5f);

        struct v2 offset = v2_add(center, v2(width * RandomNormal(&series), height * RandomNormal(&series)));

        DrawBitmap(buffer, stamp, offset, stampCenter);
      }
    }
  }

  for (i32 chunkOffsetY = -1; chunkOffsetY <= 1; chunkOffsetY++) {
    for (i32 chunkOffsetX = -1; chunkOffsetX <= 1; chunkOffsetX++) {
      u32 chunkX = chunkPosition->chunkX + (u32)chunkOffsetX;
      u32 chunkY = chunkPosition->chunkY + (u32)chunkOffsetY;
      u32 chunkZ = chunkPosition->chunkZ;

      u32 seed = 139 * chunkX + 593 * chunkY + 329 * chunkZ;
      struct random_series series = RandomSeed(seed);

      struct v2 center = v2((f32)chunkOffsetX * width, (f32)chunkOffsetY * width);
      center.y = -center.y; /* y flipped for draw */

      for (u32 tuftIndex = 0; tuftIndex < 30; tuftIndex++) {
        struct bitmap *tuft = state->textureTuft + RandomChoice(&series, ARRAY_COUNT(state->textureTuft));
        struct v2 tuftCenter = v2_mul(v2u(tuft->width, tuft->height), 0.5f);

        struct v2 offset = v2_add(center, v2(width * RandomNormal(&series), height * RandomNormal(&series)));

        DrawBitmap(buffer, tuft, offset, tuftCenter);
      }
    }
  }
}

void
GameUpdateAndRender(struct game_memory *memory, struct game_input *input, struct game_backbuffer *backbuffer)
{
  assert(sizeof(struct game_state) <= memory->permanentStorageSize);
  struct game_state *state = memory->permanentStorage;
  f32 dt = input->dtPerFrame;

  u32 groundBufferWidth = 256;
  u32 groundBufferHeight = 256;
  /****************************************************************
   * INITIALIZATION
   ****************************************************************/
  if (!memory->initialized) {
    void *data = memory->permanentStorage + sizeof(*state);
    memory_arena_size_t size = memory->permanentStorageSize - sizeof(*state);
    MemoryArenaInit(&state->worldArena, data, size);

    /* world creation */
    struct world *world = MemoryArenaPush(&state->worldArena, sizeof(*world));
    state->world = world;

    state->floorHeight = 3.0f;

    /* unit: pixels/meters */
    state->metersToPixels = 42.0f;
    /* unit: meters/pixels */
    state->pixelsToMeters = 1.0f / state->metersToPixels;

    struct v3 chunkDimInMeters = {
        state->pixelsToMeters * (f32)groundBufferWidth,
        state->pixelsToMeters * (f32)groundBufferHeight,
        state->floorHeight,
    };
    WorldInit(world, chunkDimInMeters);

    /* collision groups */
    f32 tileSideInMeters = 1.4f;
    f32 tileDepthInMeters = state->floorHeight;

    state->heroCollision = MakeSimpleGroundedCollision(&state->worldArena, v3(1.0f, 0.5f, 1.2f));
    state->familiarCollision = MakeSimpleGroundedCollision(&state->worldArena, v3(1.0f, 0.5f, 0.5f));
    state->monsterCollision = MakeSimpleGroundedCollision(&state->worldArena, v3(1.0f, 0.5f, 0.5f));
    state->wallCollision =
        MakeSimpleGroundedCollision(&state->worldArena, v3(tileSideInMeters, tileSideInMeters, tileDepthInMeters));
    state->swordCollision =
        MakeSimpleGroundedCollision(&state->worldArena, v3(tileSideInMeters, tileSideInMeters, 0.1f));
    state->stairwellCollision = MakeSimpleGroundedCollision(
        &state->worldArena, v3(tileSideInMeters, 2.0f * tileSideInMeters, 1.1f * tileDepthInMeters));
    state->roomCollision = MakeSimpleGroundedCollision(&state->worldArena, v3(tileSideInMeters * (f32)TILES_PER_WIDTH,
                                                                              tileSideInMeters * (f32)TILES_PER_HEIGHT,
                                                                              0.9f * tileDepthInMeters));

    /* load grass */
    state->textureGrass[0] = LoadBmp(memory->PlatformReadEntireFile, "test2/grass00.bmp");
    state->textureGrass[1] = LoadBmp(memory->PlatformReadEntireFile, "test2/grass01.bmp");

    state->textureTuft[0] = LoadBmp(memory->PlatformReadEntireFile, "test2/tuft00.bmp");
    state->textureTuft[1] = LoadBmp(memory->PlatformReadEntireFile, "test2/tuft01.bmp");
    state->textureTuft[2] = LoadBmp(memory->PlatformReadEntireFile, "test2/tuft02.bmp");

    state->textureGround[0] = LoadBmp(memory->PlatformReadEntireFile, "test2/ground00.bmp");
    state->textureGround[1] = LoadBmp(memory->PlatformReadEntireFile, "test2/ground01.bmp");
    state->textureGround[2] = LoadBmp(memory->PlatformReadEntireFile, "test2/ground02.bmp");
    state->textureGround[3] = LoadBmp(memory->PlatformReadEntireFile, "test2/ground03.bmp");

    /* load background */
    state->textureBackground = LoadBmp(memory->PlatformReadEntireFile, "test/test_background.bmp");

    /* load shadow */
    state->textureShadow = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_shadow.bmp");

    state->textureTree = LoadBmp(memory->PlatformReadEntireFile, "test2/tree00.bmp");

    state->textureSword = LoadBmp(memory->PlatformReadEntireFile, "test2/rock03.bmp");

    state->textureStairwell = LoadBmp(memory->PlatformReadEntireFile, "test2/rock02.bmp");

    /* load hero bitmaps */
    struct bitmap_hero *bitmapHero = &state->textureHero[BITMAP_HERO_FRONT];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_front_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_front_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_front_cape.bmp");
    bitmapHero->align = v2(72, 182);

    bitmapHero = &state->textureHero[BITMAP_HERO_BACK];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_back_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_back_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_back_cape.bmp");
    bitmapHero->align = v2(72, 182);

    bitmapHero = &state->textureHero[BITMAP_HERO_LEFT];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_left_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_left_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_left_cape.bmp");
    bitmapHero->align = v2(72, 182);

    bitmapHero = &state->textureHero[BITMAP_HERO_RIGHT];
    bitmapHero->head = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_right_head.bmp");
    bitmapHero->torso = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_right_torso.bmp");
    bitmapHero->cape = LoadBmp(memory->PlatformReadEntireFile, "test/test_hero_right_cape.bmp");
    bitmapHero->align = v2(72, 182);

    /* use entity with 0 index as null */
    StoredEntityAdd(state, ENTITY_TYPE_INVALID, 0, 0);

    /* generate procedural tile map */
    u32 screenBaseX = (U16_MAX / TILES_PER_WIDTH) / 2;
    u32 screenBaseY = (U16_MAX / TILES_PER_HEIGHT) / 2;
    u32 screenBaseZ = U16_MAX / 2;
    u32 screenX = screenBaseX;
    u32 screenY = screenBaseY;
    u32 absTileZ = screenBaseZ;

    u8 isDoorLeft = 0;
    u8 isDoorRight = 0;
    u8 isDoorTop = 0;
    u8 isDoorBottom = 0;
    u8 isDoorUp = 0;
    u8 isDoorDown = 0;

    struct random_series series = RandomSeed(1234);
    for (u32 screenIndex = 0; screenIndex < 2000; screenIndex++) {
      // u32 choice = RandomChoice(&series, (isDoorUp || isDoorDown) ? 2 : 3);
      u32 choice = RandomChoice(&series, 2);

      u8 isDoorZ = 0;
      if (choice == 2) {
        isDoorZ = 1;
        if (absTileZ == screenBaseZ)
          isDoorUp = 1;
        else
          isDoorDown = 1;
      } else if (choice == 1)
        isDoorRight = 1;
      else
        isDoorTop = 1;

      SpaceAdd(state, (screenX * TILES_PER_WIDTH + TILES_PER_WIDTH / 2),
               (screenY * TILES_PER_HEIGHT + TILES_PER_HEIGHT / 2), absTileZ);

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
            if (tileX == 10 && tileY == 5) {
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

      if (choice == 2)
        if (absTileZ == screenBaseZ)
          absTileZ = screenBaseZ + 1;
        else
          absTileZ = screenBaseZ;
      else if (choice == 1)
        screenX += 1;
      else
        screenY += 1;
    }

    /* set initial camera position */
    u32 initialCameraX = screenBaseX * TILES_PER_WIDTH + TILES_PER_WIDTH / 2;
    u32 initialCameraY = screenBaseY * TILES_PER_HEIGHT + TILES_PER_HEIGHT / 2;
    u32 initialCameraZ = screenBaseZ;

    for (u32 monsterIndex = 0; monsterIndex < 3; monsterIndex++) {
      i32 monsterOffsetX = RandomBetweenI32(&series, -7, 7);
      i32 monsterOffsetY = RandomBetweenI32(&series, 1, 3);

      u32 monsterX = initialCameraX + (u32)monsterOffsetX;
      u32 monsterY = initialCameraY + (u32)monsterOffsetY;
      MonsterAdd(state, monsterX, monsterY, initialCameraZ);
    }

    for (u32 familiarIndex = 0; familiarIndex < 1; familiarIndex++) {
      i32 familiarOffsetX = RandomBetweenI32(&series, -7, 7);
      i32 familiarOffsetY = RandomBetweenI32(&series, -3, -1);

      u32 familiarX = initialCameraX + (u32)familiarOffsetX;
      u32 familiarY = initialCameraY + (u32)familiarOffsetY;
      FamiliarAdd(state, familiarX, familiarY, initialCameraZ);
    }

    struct world_position initialCameraPosition =
        ChunkPositionFromTilePosition(state->world, initialCameraX, initialCameraY, initialCameraZ);
    state->cameraPosition = initialCameraPosition;

    memory->initialized = 1;
  }

  struct world *world = state->world;
  assert(world);

  /****************************************************************
   * TRANSIENT STATE INITIALIZATION
   ****************************************************************/
  assert(sizeof(struct transient_state) <= memory->transientStorageSize);
  struct transient_state *transientState = memory->transientStorage;
  if (!transientState->initialized) {
    void *data = memory->transientStorage + sizeof(*transientState);
    memory_arena_size_t size = memory->transientStorageSize - sizeof(*transientState);
    MemoryArenaInit(&transientState->transientArena, data, size);

    /* cache composited ground drawing */
    // TODO(e2dk4r): pick a real value here
    transientState->groundBufferCount = 64; // 128;
    transientState->groundBuffers = MemoryArenaPush(
        &transientState->transientArena, sizeof(*transientState->groundBuffers) * transientState->groundBufferCount);
    for (u32 groundBufferIndex = 0; groundBufferIndex < transientState->groundBufferCount; groundBufferIndex++) {
      struct ground_buffer *groundBuffer = transientState->groundBuffers + groundBufferIndex;

      groundBuffer->bitmap = MakeEmptyBitmap(&transientState->transientArena, groundBufferWidth, groundBufferHeight);
      groundBuffer->position = WorldPositionInvalid();
    }

    transientState->initialized = 1;
  }

#if HANDMADEHERO_DEBUG
  if (input->gameCodeReloaded) {
    for (u32 groundBufferIndex = 0; groundBufferIndex < transientState->groundBufferCount; groundBufferIndex++) {
      struct ground_buffer *groundBuffer = transientState->groundBuffers + groundBufferIndex;
      groundBuffer->position = WorldPositionInvalid();
    }
  }
#endif

  /****************************************************************
   * CONTROLLER INPUT HANDLING
   ****************************************************************/

  for (u8 controllerIndex = 0; controllerIndex < ARRAY_COUNT(input->controllers); controllerIndex++) {
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

  /****************************************************************
   * RENDERING
   ****************************************************************/
  f32 metersToPixels = state->metersToPixels;
  f32 pixelsToMeters = state->pixelsToMeters;
  struct bitmap *drawBuffer = (struct bitmap *)backbuffer;

  struct memory_temp renderMemory = BeginTemporaryMemory(&transientState->transientArena);
  struct render_group *renderGroup = RenderGroup(&transientState->transientArena, 4 * MEGABYTES, state->metersToPixels);

/* drawing background */
#if 0
  DrawBitmap(drawBuffer, &state->bitmapBackground, v2(0.0f, 0.0f), v2(0, 0));
#else
  struct v4 backgroundColor = v4(0.5f, 0.5f, 0.5f, 1.0f);
  DrawRectangle(drawBuffer, v2(0.0f, 0.0f), v2((f32)drawBuffer->width, (f32)drawBuffer->height), &backgroundColor);
#endif

  struct v2 screenDim = {
      .x = (f32)backbuffer->width,
      .y = (f32)backbuffer->height,
  };
  struct v2 screenCenter = v2_mul(screenDim, 0.5f);

  struct v2 screenDimInMeters = v2_mul(screenDim, pixelsToMeters);
  struct rect cameraBoundsInMeters = RectCenterDim(v3(0.0f, 0.0f, 0.0f), v2_to_v3(screenDimInMeters, 0.0f));

  /* draw ground buffer chunks */
  for (u32 groundBufferIndex = 0; groundBufferIndex < transientState->groundBufferCount; groundBufferIndex++) {
    struct ground_buffer *groundBuffer = transientState->groundBuffers + groundBufferIndex;

    if (IsGroundBufferEmpty(groundBuffer))
      continue;

    struct bitmap *bitmap = &groundBuffer->bitmap;
    struct v3 positionRelativeToCamera = WorldPositionSub(world, &groundBuffer->position, &state->cameraPosition);
    struct v4 color = v4(1.0f, 1.0f, 0.0f, 1.0f);
    f32 thickness = 1.0f;
    PushBitmap(renderGroup, bitmap, positionRelativeToCamera.xy, positionRelativeToCamera.z,
               v2_mul(v2u(bitmap->width, bitmap->height), 0.5f));
  }

  /* fill ground buffer chunks */
  {
    struct world_position minChunkPosition =
        WorldPositionCalculate(world, &state->cameraPosition, RectMin(&cameraBoundsInMeters));
    struct world_position maxChunkPosition =
        WorldPositionCalculate(world, &state->cameraPosition, RectMax(&cameraBoundsInMeters));
    for (u32 chunkZ = minChunkPosition.chunkZ; chunkZ <= maxChunkPosition.chunkZ; chunkZ++) {
      for (u32 chunkY = minChunkPosition.chunkY; chunkY <= maxChunkPosition.chunkY; chunkY++) {
        for (u32 chunkX = minChunkPosition.chunkX; chunkX <= maxChunkPosition.chunkX; chunkX++) {
          struct world_position chunkCenterPosition = WorldPositionCentered(chunkX, chunkY, chunkZ);

          // TODO(e2dk4r): this is super inefficient, fix it!
          f32 furthestBufferLengthSq = 0.0f;
          struct ground_buffer *furthestBuffer = 0;
          for (u32 groundBufferIndex = 0; groundBufferIndex < transientState->groundBufferCount; groundBufferIndex++) {
            struct ground_buffer *groundBuffer = transientState->groundBuffers + groundBufferIndex;

            if (IsWorldPositionSame(world, &groundBuffer->position, &chunkCenterPosition)) {
              furthestBuffer = 0;
              break;
            } else if (IsGroundBufferEmpty(groundBuffer)) {
              furthestBufferLengthSq = F32_MAX;
              furthestBuffer = groundBuffer;
            } else {
              struct v3 rel = WorldPositionSub(world, &groundBuffer->position, &state->cameraPosition);
              f32 farSq = v2_length_square(rel.xy);
              if (furthestBufferLengthSq >= farSq)
                continue;

              furthestBufferLengthSq = farSq;
              furthestBuffer = groundBuffer;
            }
          }

          if (furthestBuffer) {
            FillGroundChunk(transientState, state, furthestBuffer, &chunkCenterPosition);
          }
        }
      }
    }
  }

  struct memory_temp simRegionMemory = BeginTemporaryMemory(&transientState->transientArena);
  struct v3 simBoundExpansion = v3(15.0f, 15.0f, 15.0f);
  struct rect simBounds = RectAddRadius(&cameraBoundsInMeters, simBoundExpansion);
  struct sim_region *simRegion =
      BeginSimRegion(&transientState->transientArena, state, state->world, state->cameraPosition, simBounds, dt);

  /* draw ground */
  for (u32 groundBufferIndex = 0; groundBufferIndex < transientState->groundBufferCount; groundBufferIndex++) {
    struct ground_buffer *groundBuffer = transientState->groundBuffers + groundBufferIndex;

    if (IsGroundBufferEmpty(groundBuffer))
      continue;

    struct bitmap *bitmap = &groundBuffer->bitmap;

    struct v3 delta = WorldPositionSub(state->world, &groundBuffer->position, &state->cameraPosition);
    delta.y = -delta.y;
    v3_mul_ref(&delta, metersToPixels);

    struct v2 position = v2_sub(screenCenter, v2_mul(v2u(bitmap->width, bitmap->height), 0.5f));
    v2_add_ref(&position, delta.xy);

    DrawBitmap(drawBuffer, bitmap, position, v2(0.0f, 0.0f));
  }

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

    struct render_basis *basis = MemoryArenaPush(&transientState->transientArena, sizeof(*basis));
    basis->position = entity->position;
    renderGroup->defaultBasis = basis;

    struct v2 entityScreenPosition = entity->position.xy;
    /* screen's coordinate system uses y values inverse,
     * so that means going up in space means negative y values
     */
    entityScreenPosition.y *= -1;
    v2_mul_ref(&entityScreenPosition, metersToPixels);

    struct v2 entityGroundPoint = v2_add(screenCenter, entityScreenPosition);

    if (entity->type & ENTITY_TYPE_HERO) {
      /* update */
      for (u8 controllerIndex = 0; controllerIndex < ARRAY_COUNT(state->controlledHeroes); controllerIndex++) {
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
      struct bitmap_hero *bitmap = &state->textureHero[entity->facingDirection];

      // TODO(e2dk4r): this is incorrect, should be computed after update!
      f32 shadowAlpha = 1.0f - entity->position.z;
      if (shadowAlpha < 0.0f)
        shadowAlpha = 0.0f;

      PushHitPoints(renderGroup, entity);

      PushBitmapWithAlphaAndZ(renderGroup, &state->textureShadow, v2(0.0f, 0.0f), 0.0f, bitmap->align, shadowAlpha,
                              0.0f);
      PushBitmap(renderGroup, &bitmap->torso, v2(0.0f, 0.0f), 0.0f, bitmap->align);
      PushBitmap(renderGroup, &bitmap->cape, v2(0.0f, 0.0f), 0.0f, bitmap->align);
      PushBitmap(renderGroup, &bitmap->head, v2(0.0f, 0.0f), 0.0f, bitmap->align);
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
      struct bitmap_hero *bitmap = &state->textureHero[entity->facingDirection];

      f32 bobSin = Sin(2.0f * entity->tBob);
      f32 shadowAlpha = (0.5f * 1.0f) - (0.2f * bobSin);

      PushBitmapWithAlpha(renderGroup, &state->textureShadow, v2(0.0f, 0.0f), 0.0f, bitmap->align, shadowAlpha);
      PushBitmap(renderGroup, &bitmap->head, v2(0.0f, 0.0f), 0.25f * bobSin, bitmap->align);
    }

    else if (entity->type & ENTITY_TYPE_MONSTER) {
      /* render */
      struct bitmap_hero *bitmap = &state->textureHero[entity->facingDirection];
      f32 alpha = 1.0f;

      PushHitPoints(renderGroup, entity);
      PushBitmapWithAlpha(renderGroup, &state->textureShadow, v2(0.0f, 0.0f), 0.0f, bitmap->align, alpha);
      PushBitmap(renderGroup, &bitmap->torso, v2(0.0f, 0.0f), 0.0f, bitmap->align);
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
      PushBitmapWithAlpha(renderGroup, &state->textureShadow, v2(0.0f, 0.0f), 0.0f, v2(72.0f, 182.0f), 1.0f);
      PushBitmap(renderGroup, &state->textureSword, v2(0.0f, 0.0f), 0.0f, v2(29.0f, 10.0f));
    }

    else if (entity->type & ENTITY_TYPE_WALL) {
#if 1
      PushBitmap(renderGroup, &state->textureTree, v2(0.0f, 0.0f), 0.0f, v2(40.0f, 80.0f));
#else
      for (u32 entityVolumeIndex = 0; entity->collision && entityVolumeIndex < entity->collision->volumeCount;
           entityVolumeIndex++) {
        struct entity_collision_volume *entityVolume = entity->collision->volumes + entityVolumeIndex;

        PushRect(renderGroup, v2(0.0f, 0.0f), 0.0f, entityVolume->dim.xy, v4(1.0f, 1.0f, 0.0f, 1.0f));
      }
#endif
    }

    else if (entity->type & ENTITY_TYPE_STAIRWELL) {
      PushRect(renderGroup, v2(0.0f, 0.0f), 0.0f, entity->walkableDim.xy, v4(1.0f, 0.5f, 0.0f, 1.0f));
      PushRect(renderGroup, v2(0.0f, 0.0f), entity->walkableHeight, entity->walkableDim.xy, v4(1.0f, 1.0f, 0.0f, 1.0f));
    }

    else if (entity->type & ENTITY_TYPE_SPACE) {
#if 0
      for (u32 entityVolumeIndex = 0; entity->collision && entityVolumeIndex < entity->collision->volumeCount;
           entityVolumeIndex++) {
        struct entity_collision_volume *entityVolume = entity->collision->volumes + entityVolumeIndex;
        PushRectOutline(renderGroup, entityVolume->offset.xy, 0.0f, entityVolume->dim.xy, v4(0.0f, 0.5f, 1.0f, 1.0f));
      }
#endif
    }

    else {
      assert(0 && "unknown entity type");
    }
  }

  for (u32 pushBufferIndex = 0; pushBufferIndex < renderGroup->pushBufferSize;) {
    struct render_entity *renderEntity = renderGroup->pushBufferBase + pushBufferIndex;
    pushBufferIndex += sizeof(*renderEntity);

    struct v3 entityBasePosition = renderEntity->basis->position;

    entityBasePosition.y += renderEntity->offsetZ;
    /* screen's coordinate system uses y values inverse,
     * so that means going up in space means negative y values
     */
    entityBasePosition.y *= -1;
    v2_mul_ref(&entityBasePosition.xy, metersToPixels);

    f32 entityZ = -entityBasePosition.z * metersToPixels;

    struct v2 entityGroundPoint = v2_add(screenCenter, entityBasePosition.xy);
    struct v2 center = v2_add(entityGroundPoint, renderEntity->offset);
    center.y += renderEntity->cZ * entityZ;

    if (renderEntity->bitmap) {
      DrawBitmap2(drawBuffer, renderEntity->bitmap, center, renderEntity->align, renderEntity->color.a);
    } else {
      struct v2 halfDim = v2_mul(v2_mul(renderEntity->dim, 0.5f), metersToPixels);
      DrawRectangle(drawBuffer, v2_sub(center, halfDim), v2_add(center, halfDim), &renderEntity->color);
    }
  }

  EndSimRegion(simRegion, state);
  EndTemporaryMemory(&simRegionMemory);
  EndTemporaryMemory(&renderMemory);

  MemoryArenaCheck(&state->worldArena);
  MemoryArenaCheck(&transientState->transientArena);
}
