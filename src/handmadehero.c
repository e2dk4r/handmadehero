#include <handmadehero/assert.h>
#include <handmadehero/atomic.h>
#include <handmadehero/color.h>
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
  entity->storageIndex = storedEntityIndex;
  entity->type = type;
  entity->collision = collision;

  EntityChangeLocation(&state->worldArena, state->world, stored, position);

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
  struct entity *hero = &stored->sim;

  EntityAddFlag(hero, ENTITY_FLAG_COLLIDE);
  EntityHitPointsReset(hero, 3);

  u32 swordIndex = SwordAdd(state);
  hero->sword.index = swordIndex;
  CollisionRuleAdd(state, hero->storageIndex, hero->sword.index, 0);

  /* if followed entity, does not exits */
  if (state->followedEntityIndex == 0)
    state->followedEntityIndex = hero->storageIndex;

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

  EntityAddFlag(entity, ENTITY_FLAG_COLLIDE);
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
HitPoints(struct render_group *renderGroup, struct entity *entity)
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

    Rect(renderGroup, v2_to_v3(hitPosition, 0.0f), healthDim, color);

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
      .stride = (s32)width * BITMAP_BYTES_PER_PIXEL,
      .memory = MemoryArenaPushAlignment(arena, totalBitmapSize, 16),
      .alignPercentage = v2(0.5f, 0.5f),
      .widthOverHeight = (f32)width / (f32)height,
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

internal void
MakeSphereDiffuseMap(struct bitmap *bitmap)
{
  f32 invWidth = 1.0f / ((f32)bitmap->width - 1.0f);
  f32 invHeight = 1.0f / ((f32)bitmap->height - 1.0f);

  u8 *row = bitmap->memory;
  for (u32 y = 0; y < bitmap->height; y++) {
    u32 *pixel = (u32 *)row;
    for (u32 x = 0; x < bitmap->width; x++) {
      struct v2 bitmapUV = v2((f32)x * invWidth, (f32)y * invHeight);

      f32 Nx = 2.0f * bitmapUV.x - 1.0f;
      f32 Ny = 2.0f * bitmapUV.y - 1.0f;

      f32 rootTerm = 1.0f - Square(Nx) - Square(Ny);
      f32 alpha = 0.0f;
      if (rootTerm >= 0.0f) {
        alpha = 1.0f;
      }

      struct v3 baseColor = v3(0.0f, 0.0f, 0.0f);
      alpha *= 255.0f;
      struct v4 color = v4(alpha * baseColor.r, alpha * baseColor.g, alpha * baseColor.b, alpha);

      *pixel = (u32)(color.a + 0.5f) << 0x18 | (u32)(color.r + 0.5f) << 0x10 | (u32)(color.g + 0.5f) << 0x08 |
               (u32)(color.b + 0.5f) << 0x00;
      pixel++;
    }

    row += bitmap->stride;
  }
}

internal void
MakeSphereNormalMap(struct bitmap *bitmap, f32 roughness)
{
  f32 invWidth = 1.0f / ((f32)bitmap->width - 1.0f);
  f32 invHeight = 1.0f / ((f32)bitmap->height - 1.0f);

  u8 *row = bitmap->memory;
  for (u32 y = 0; y < bitmap->height; y++) {
    u32 *pixel = (u32 *)row;
    for (u32 x = 0; x < bitmap->width; x++) {
      struct v2 bitmapUV = v2((f32)x * invWidth, (f32)y * invHeight);

      f32 Nx = 2.0f * bitmapUV.x - 1.0f;
      f32 Ny = 2.0f * bitmapUV.y - 1.0f;

      struct v3 normal = v3(0.0f, 0.70710678118655f, 0.70710678118655f);

      f32 rootTerm = 1.0f - Square(Nx) - Square(Ny);
      if (rootTerm >= 0.0f) {
        f32 Nz = SquareRoot(rootTerm);
        normal = v3(Nx, Ny, Nz);
      }

      struct v4 color = {.x = 255.0f * 0.5f * (normal.x + 1.0f),
                         .y = 255.0f * 0.5f * (normal.y + 1.0f),
                         .z = 255.0f * 0.5f * (normal.z + 1.0f),
                         .w = 255.0f * roughness};

      *pixel = (u32)(color.a + 0.5f) << 0x18 | (u32)(color.r + 0.5f) << 0x10 | (u32)(color.g + 0.5f) << 0x08 |
               (u32)(color.b + 0.5f) << 0x00;
      pixel++;
    }

    row += bitmap->stride;
  }
}

internal void
MakeCylinderNormalMapX(struct bitmap *bitmap, f32 roughness)
{
  f32 invWidth = 1.0f / ((f32)bitmap->width - 1.0f);
  f32 invHeight = 1.0f / ((f32)bitmap->height - 1.0f);

  u8 *row = bitmap->memory;
  for (u32 y = 0; y < bitmap->height; y++) {
    u32 *pixel = (u32 *)row;
    for (u32 x = 0; x < bitmap->width; x++) {
      struct v2 bitmapUV = v2((f32)x * invWidth, (f32)y * invHeight);

      f32 Nx = 0;
      f32 Ny = 2.0f * bitmapUV.y - 1.0f;
      f32 Nz = SquareRoot(1.0f - Square(Nx));
      struct v3 normal = v3(Nx, Ny, Nz);

      struct v4 color = {.x = 255.0f * 0.5f * (normal.x + 1.0f),
                         .y = 255.0f * 0.5f * (normal.y + 1.0f),
                         .z = 255.0f * 0.5f * (normal.z + 1.0f),
                         .w = 255.0f * roughness};

      *pixel = (u32)(color.a + 0.5f) << 0x18 | (u32)(color.r + 0.5f) << 0x10 | (u32)(color.g + 0.5f) << 0x08 |
               (u32)(color.b + 0.5f) << 0x00;
      pixel++;
    }

    row += bitmap->stride;
  }
}

internal void
MakePyramidNormalMap(struct bitmap *bitmap, f32 roughness)
{
  f32 invWidth = 1.0f / ((f32)bitmap->width - 1.0f);
  f32 invHeight = 1.0f / ((f32)bitmap->height - 1.0f);

  u8 *row = bitmap->memory;
  for (u32 y = 0; y < bitmap->height; y++) {
    u32 *pixel = (u32 *)row;
    for (u32 x = 0; x < bitmap->width; x++) {
      struct v2 bitmapUV = v2((f32)x * invWidth, (f32)y * invHeight);

      f32 seven = 0.70710678118655f;
      struct v3 normal = v3(0.0f, 0.0f, seven);

      u32 invX = (bitmap->width - 1) - x;
      if (x < y) {
        if (invX < y)
          normal.x = -seven;
        else
          normal.y = seven;
      } else {
        if (invX < y)
          normal.y = -seven;
        else
          normal.x = seven;
      }

      struct v4 color = {.x = 255.0f * 0.5f * (normal.x + 1.0f),
                         .y = 255.0f * 0.5f * (normal.y + 1.0f),
                         .z = 255.0f * 0.5f * (normal.z + 1.0f),
                         .w = 255.0f * roughness};

      *pixel = (u32)(color.a + 0.5f) << 0x18 | (u32)(color.r + 0.5f) << 0x10 | (u32)(color.g + 0.5f) << 0x08 |
               (u32)(color.b + 0.5f) << 0x00;
      pixel++;
    }

    row += bitmap->stride;
  }
}

internal inline u8
IsGroundBufferEmpty(struct ground_buffer *groundBuffer)
{
  return !WorldPositionIsValid(&groundBuffer->position);
}

struct task_with_memory *
BeginTaskWithMemory(struct transient_state *transientState)
{
  struct task_with_memory *foundTask = 0;
  for (u32 taskIndex = 0; taskIndex < ARRAY_COUNT(transientState->tasks); taskIndex++) {
    struct task_with_memory *task = transientState->tasks + taskIndex;

    if (!task->isUsed) {
      foundTask = task;
      foundTask->memoryFlush = BeginTemporaryMemory(&foundTask->arena);
      foundTask->isUsed = 1;
      break;
    }
  }

  return foundTask;
}

void
EndTaskWithMemory(struct task_with_memory *task)
{
  EndTemporaryMemory(&task->memoryFlush);
  task->isUsed = 0;
}

struct fill_ground_chunk_work {
  struct task_with_memory *task;
  struct render_group *renderGroup;
  struct bitmap *buffer;
};

internal void
DoFillGroundChunkWork(struct platform_work_queue *queue, void *data)
{
  struct fill_ground_chunk_work *work = data;
  DrawRenderGroup(work->renderGroup, work->buffer);
  EndTaskWithMemory(work->task);
}

internal void
FillGroundChunk(struct transient_state *transientState, struct game_state *state, struct ground_buffer *groundBuffer,
                struct world_position *chunkPosition)
{
  struct task_with_memory *task = BeginTaskWithMemory(transientState);
  if (!task)
    return;
  struct fill_ground_chunk_work *work = MemoryArenaPush(&task->arena, sizeof(*work));

  struct bitmap *buffer = &groundBuffer->bitmap;

  f32 width = state->world->chunkDimInMeters.x;
  f32 height = state->world->chunkDimInMeters.y;
  assert(width == height && "warping not allowed");
  struct v2 halfDim = v2_mul(state->world->chunkDimInMeters.xy, 0.5f);

  // TODO(e2dk4r): Pushbuffer size?
  struct render_group *renderGroup = RenderGroup(
      &task->arena, MemoryArenaGetRemainingSize(&task->arena) - sizeof(*renderGroup), transientState->assets);
  RenderGroupOrthographic(renderGroup, buffer->width, buffer->height, (f32)(buffer->width - 2) / width);

  Clear(renderGroup, COLOR_FUCHSIA_900);

  for (s32 chunkOffsetY = -1; chunkOffsetY <= 1; chunkOffsetY++) {
    for (s32 chunkOffsetX = -1; chunkOffsetX <= 1; chunkOffsetX++) {
      u32 chunkX = chunkPosition->chunkX + (u32)chunkOffsetX;
      u32 chunkY = chunkPosition->chunkY + (u32)chunkOffsetY;
      u32 chunkZ = chunkPosition->chunkZ;

      u32 seed = 139 * chunkX + 593 * chunkY + 329 * chunkZ;
      struct random_series series = RandomSeed(seed);

#if 0
      struct v4 color = COLOR_RED_500;
      if (chunkX % 2 == chunkY % 2)
        color = COLOR_BLUE_500;
#else
      struct v4 color = v4(1.0f, 1.0f, 1.0f, 1.0f);
#endif

      struct v2 center = v2((f32)chunkOffsetX * width, (f32)chunkOffsetY * height);

      for (u32 grassIndex = 0; grassIndex < 100; grassIndex++) {
        struct bitmap_id stamp;
        if (RandomChoice(&series, 2))
          stamp = RandomBitmap(&series, transientState->assets, ASSET_TYPE_GRASS);
        else
          stamp = RandomBitmap(&series, transientState->assets, ASSET_TYPE_GROUND);

        struct v2 position = center;
        v2_add_ref(&position, v2_hadamard(halfDim, v2(RandomUnit(&series), RandomUnit(&series))));

        BitmapAsset(renderGroup, stamp, v2_to_v3(position, 0.0f), 2.0f, color);
      }
    }
  }

  for (s32 chunkOffsetY = -1; chunkOffsetY <= 1; chunkOffsetY++) {
    for (s32 chunkOffsetX = -1; chunkOffsetX <= 1; chunkOffsetX++) {
      u32 chunkX = chunkPosition->chunkX + (u32)chunkOffsetX;
      u32 chunkY = chunkPosition->chunkY + (u32)chunkOffsetY;
      u32 chunkZ = chunkPosition->chunkZ;

      u32 seed = 139 * chunkX + 593 * chunkY + 329 * chunkZ;
      struct random_series series = RandomSeed(seed);

      struct v2 center = v2((f32)chunkOffsetX * width, (f32)chunkOffsetY * height);

      for (u32 tuftIndex = 0; tuftIndex < 30; tuftIndex++) {
        struct bitmap_id tuft = RandomBitmap(&series, transientState->assets, ASSET_TYPE_TUFT);

        struct v2 position = center;
        v2_add_ref(&position, v2_hadamard(halfDim, v2(RandomUnit(&series), RandomUnit(&series))));

        BitmapAsset(renderGroup, tuft, v2_to_v3(position, 0.0f), 0.1f, v4(1.0f, 1.0f, 1.0f, 1.0f));
      }
    }
  }

  if (!RenderGroupIsAllResourcesPreset(renderGroup)) {
    // do not blit, until all resources loaded into memory
    EndTaskWithMemory(task);
    return;
  }

  groundBuffer->position = *chunkPosition;

  work->task = task;
  work->renderGroup = renderGroup;
  work->buffer = &groundBuffer->bitmap;
  Platform->WorkQueueAddEntry(transientState->lowPriorityQueue, DoFillGroundChunkWork, work);
}

#if HANDMADEHERO_INTERNAL
struct game_memory *DEBUG_GLOBAL_MEMORY;
#endif

b32
GameOutputAudio(struct game_memory *memory, struct game_audio_buffer *audioBuffer)
{
  struct game_state *state = memory->permanentStorage;
  struct transient_state *transientState = memory->transientStorage;
  b32 isWritten = 0;
  if (!state->isInitialized || !transientState->isInitialized)
    return isWritten;
  if (audioBuffer->sampleCount == 0)
    return isWritten;

  isWritten = OutputPlayingAudios(&state->audioState, audioBuffer, transientState->assets);

  return isWritten;
}

struct platform_api *Platform;
void
GameUpdateAndRender(struct game_memory *memory, struct game_input *input, struct game_backbuffer *backbuffer)
{
#if HANDMADEHERO_INTERNAL
  DEBUG_GLOBAL_MEMORY = memory;
#endif
  assert(memory->highPriorityQueue && "platform layer NOT provided high priority queue implementation");
  assert(memory->lowPriorityQueue && "platform layer NOT provided low priority queue implementation");

  Platform = &memory->platform;
  assert(Platform->WorkQueueAddEntry && "platform layer NOT implemented PlatformWorkQueueAddEntry");
  assert(Platform->WorkQueueCompleteAllWork && "platform layer NOT implemented PlatformWorkQueueCompleteAllWork");
  assert(Platform->OpenNextFile && "platform layer NOT implemented PlatformOpenFile");
  assert(Platform->ReadFromFile && "platform layer NOT implemented PlatformReadFromFile");
  assert(Platform->GetAllFilesOfTypeBegin && "platform layer NOT implemented PlatformGetAllFilesOfTypeBegin");
  assert(Platform->HasFileError && "platform layer NOT implemented PlatformHasFileError");
  assert(Platform->FileError && "platform layer NOT implemented PlatformFileError");
  assert(Platform->GetAllFilesOfTypeEnd && "platform layer NOT implemented PlatformGetAllFilesOfTypeEnd");

  BEGIN_TIMER_BLOCK(GameUpdateAndRender);

  assert(sizeof(struct game_state) <= memory->permanentStorageSize);
  struct game_state *state = memory->permanentStorage;
  f32 dt = input->dtPerFrame;

  u32 groundBufferWidth = 256;
  u32 groundBufferHeight = 256;
  /****************************************************************
   * INITIALIZATION
   ****************************************************************/
  if (!state->isInitialized) {
    state->generalEntropy = RandomSeed(1234);

    void *data = memory->permanentStorage + sizeof(*state);
    memory_arena_size_t size = memory->permanentStorageSize - sizeof(*state);
    MemoryArenaInit(&state->worldArena, data, size);

    MemorySubArenaInit(&state->metaArena, &state->worldArena, 2 * MEGABYTES);
    AudioStateInit(&state->audioState, &state->metaArena);

    /* world creation */
    struct world *world = MemoryArenaPush(&state->worldArena, sizeof(*world));
    state->world = world;

    state->floorHeight = 3.0f;

    // TODO(e2dk4r): Remove this!
    /* unit: meters/pixels */
    f32 pixelsToMeters = 1.0f / 42.0f;
    struct v3 chunkDimInMeters = {
        pixelsToMeters * (f32)groundBufferWidth,
        pixelsToMeters * (f32)groundBufferHeight,
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
#if 1
      u32 choice = RandomChoice(&series, (isDoorUp || isDoorDown) ? 2 : 4);
#else
      u32 choice = RandomChoice(&series, 2);
#endif

      // choice = 3;

      u8 isDoorZ = 0;
      if (choice == 3) {
        isDoorZ = 1;
        isDoorDown = 1;
      } else if (choice == 2) {
        isDoorZ = 1;
        isDoorUp = 1;
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
            if ((absTileZ % 2 && tileX == 10 && tileY == 5) || (!(absTileZ % 2) && tileX == 4 && tileY == 5)) {
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

      if (choice == 3)
        absTileZ -= 1;
      else if (choice == 2)
        absTileZ += 1;
      else if (choice == 1)
        screenX += 1;
      else
        screenY += 1;
    }

    /* set initial camera position */
    u32 initialCameraX = screenBaseX * TILES_PER_WIDTH + TILES_PER_WIDTH / 2;
    u32 initialCameraY = screenBaseY * TILES_PER_HEIGHT + TILES_PER_HEIGHT / 2;
    u32 initialCameraZ = screenBaseZ;

    for (u32 monsterIndex = 0; monsterIndex < 1; monsterIndex++) {
      s32 monsterOffsetX = RandomBetweens32(&series, -7, 7);
      s32 monsterOffsetY = RandomBetweens32(&series, 1, 3);

      u32 monsterX = initialCameraX + (u32)monsterOffsetX;
      u32 monsterY = initialCameraY + (u32)monsterOffsetY;
      MonsterAdd(state, monsterX, monsterY, initialCameraZ);
    }

    for (u32 familiarIndex = 0; familiarIndex < 1; familiarIndex++) {
      s32 familiarOffsetX = RandomBetweens32(&series, -7, 7);
      s32 familiarOffsetY = RandomBetweens32(&series, -3, -1);

      u32 familiarX = initialCameraX + (u32)familiarOffsetX;
      u32 familiarY = initialCameraY + (u32)familiarOffsetY;
      FamiliarAdd(state, familiarX, familiarY, initialCameraZ);
    }

    struct world_position initialCameraPosition =
        ChunkPositionFromTilePosition(state->world, initialCameraX, initialCameraY, initialCameraZ);
    state->cameraPosition = initialCameraPosition;

    state->isInitialized = 1;
  }

  struct world *world = state->world;
  assert(world);

  /****************************************************************
   * TRANSIENT STATE INITIALIZATION
   ****************************************************************/
  assert(sizeof(struct transient_state) <= memory->transientStorageSize);
  struct transient_state *transientState = memory->transientStorage;
  if (!transientState->isInitialized) {
    void *data = memory->transientStorage + sizeof(*transientState);
    memory_arena_size_t size = memory->transientStorageSize - sizeof(*transientState);
    MemoryArenaInit(&transientState->transientArena, data, size);

    for (u32 taskIndex = 0; taskIndex < ARRAY_COUNT(transientState->tasks); taskIndex++) {
      struct task_with_memory *task = transientState->tasks + taskIndex;

      task->isUsed = 0;
      MemorySubArenaInit(&task->arena, &transientState->transientArena, 1 * MEGABYTES);
    }

    transientState->highPriorityQueue = memory->highPriorityQueue;
    transientState->lowPriorityQueue = memory->lowPriorityQueue;

    /* cache composited ground drawing */
    // TODO(e2dk4r): pick a real value here
    transientState->groundBufferCount = 256; // 128;
    transientState->groundBuffers = MemoryArenaPush(
        &transientState->transientArena, sizeof(*transientState->groundBuffers) * transientState->groundBufferCount);
    for (u32 groundBufferIndex = 0; groundBufferIndex < transientState->groundBufferCount; groundBufferIndex++) {
      struct ground_buffer *groundBuffer = transientState->groundBuffers + groundBufferIndex;

      groundBuffer->bitmap = MakeEmptyBitmap(&transientState->transientArena, groundBufferWidth, groundBufferHeight);
      groundBuffer->position = WorldPositionInvalid();
    }

    state->testDiffuse = MakeEmptyBitmap(&transientState->transientArena, 256, 256);
    MakeSphereDiffuseMap(&state->testDiffuse);
    // DrawRectangle(&state->testDiffuse, v2(0.0f, 0.0f), v2u(state->testDiffuse.width, state->testDiffuse.height),
    //               v4(0.5f, 0.5f, 0.5f, 1.0f));
    state->testNormal =
        MakeEmptyBitmap(&transientState->transientArena, state->testDiffuse.width, state->testDiffuse.height);
    MakeSphereNormalMap(&state->testNormal, 0.0f);

    transientState->envMapWidth = 512;
    transientState->envMapHeight = 256;
    for (u32 envMapIndex = 0; envMapIndex < ARRAY_COUNT(transientState->envMaps); envMapIndex++) {
      struct environment_map *map = transientState->envMaps + envMapIndex;
      u32 width = transientState->envMapWidth;
      u32 height = transientState->envMapHeight;
      for (u32 lodIndex = 0; lodIndex < ARRAY_COUNT(map->lod); lodIndex++) {
        assert(width != 0 && height != 0);
        map->lod[lodIndex] = MakeEmptyBitmap(&transientState->transientArena, width, height);

        width >>= 1;
        height >>= 1;
      }
    }
    transientState->envMaps[ENV_MAP_BOTTOM].z = -2.0f;
    transientState->envMaps[ENV_MAP_MIDDLE].z = 0.0f;
    transientState->envMaps[ENV_MAP_TOP].z = 2.0f;

    transientState->assets = GameAssetsAllocate(&transientState->transientArena, 64 * MEGABYTES, transientState);

#if 0
    state->music = PlayAudio(&state->audioState, AudioGetFirstId(transientState->assets, ASSET_TYPE_MUSIC));
#else
    state->music = 0;
#endif

    transientState->isInitialized = 1;
  }

#if HANDMADEHERO_DEBUG
  // TODO(e2dk4r): Re-enable this? But make sure we don't touch ones in flight?
  if (0 && input->gameCodeReloaded) {
    for (u32 groundBufferIndex = 0; groundBufferIndex < transientState->groundBufferCount; groundBufferIndex++) {
      struct ground_buffer *groundBuffer = transientState->groundBuffers + groundBufferIndex;
      groundBuffer->position = WorldPositionInvalid();
    }
  }
#endif

  /****************************************************************
   * CONTROLLER INPUT HANDLING
   ****************************************************************/

  {
    struct v2 musicVolume;
    musicVolume.e[1] = ((f32)input->pointerX / (f32)backbuffer->width);
    musicVolume.e[0] = 1.0f - musicVolume.e[1];
    ChangeVolume(&state->audioState, state->music, 0.01f, musicVolume);
  }

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
      ChangeVolume(&state->audioState, state->music, 10, v2(1.0f, 1.0f));
    } else if (controller->actionDown.pressed) {
      conHero->dSword = v2(0.0f, -1.0f);
      ChangeVolume(&state->audioState, state->music, 10, v2(0.0f, 0.0f));
    } else if (controller->actionLeft.pressed) {
      conHero->dSword = v2(-1.0f, 0.0f);
      ChangeVolume(&state->audioState, state->music, 5, v2(1.0f, 0.0f));
    } else if (controller->actionRight.pressed) {
      conHero->dSword = v2(1.0f, 0.0f);
      ChangeVolume(&state->audioState, state->music, 5, v2(0.0f, 1.0f));
    }
  }

  /****************************************************************
   * RENDERING
   ****************************************************************/
  struct bitmap drawBuffer = {
      .width = backbuffer->width,
      .height = backbuffer->height,
      .stride = (s32)backbuffer->stride,
      .memory = backbuffer->memory,
  };

  struct memory_temp renderMemory = BeginTemporaryMemory(&transientState->transientArena);
  struct render_group *renderGroup =
      RenderGroup(&transientState->transientArena, 4 * MEGABYTES, transientState->assets);
  RenderGroupPerspective(renderGroup, drawBuffer.width, drawBuffer.height);

/* drawing background */
#if 0
  Bitmap(renderGroup, &state->textureBackground, v3(0.0f, 0.0f, 0.0f), 1);
#else
  Clear(renderGroup, COLOR_ZINC_800);
#endif

  struct rect2 screenBounds = GetCameraRectangleAtTarget(renderGroup);
  struct rect cameraBoundsInMeters = RectMinMax(v2_to_v3(screenBounds.min, -3.0f * state->floorHeight),
                                                v2_to_v3(screenBounds.max, 1.0f * state->floorHeight));

  /* draw ground buffer chunks */
  for (u32 groundBufferIndex = 0; groundBufferIndex < transientState->groundBufferCount; groundBufferIndex++) {
    struct ground_buffer *groundBuffer = transientState->groundBuffers + groundBufferIndex;

    if (IsGroundBufferEmpty(groundBuffer))
      continue;

    struct v3 positionRelativeToCamera = WorldPositionSub(world, &groundBuffer->position, &state->cameraPosition);
    if (!InRange(-1.0f, 1.0f, positionRelativeToCamera.z))
      continue;

    struct bitmap *bitmap = &groundBuffer->bitmap;
    bitmap->alignPercentage = v2(0.5f, 0.5f);

    struct v2 groundDim = world->chunkDimInMeters.xy;
    Bitmap(renderGroup, bitmap, positionRelativeToCamera, groundDim.y);
#if 0
    RectOutline(renderGroup, positionRelativeToCamera, groundDim, COLOR_GRAY_500);
#endif
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

            if (IsChunkPositionSame(world, &groundBuffer->position, &chunkCenterPosition)) {
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

          if (!furthestBuffer)
            continue;

          FillGroundChunk(transientState, state, furthestBuffer, &chunkCenterPosition);
        }
      }
    }
  }

  struct memory_temp simRegionMemory = BeginTemporaryMemory(&transientState->transientArena);
  struct v3 simBoundExpansion = v3(15.0f, 15.0f, 0);
  struct rect simBounds = RectAddRadius(&cameraBoundsInMeters, simBoundExpansion);
  struct world_position simRegionOrigin = state->cameraPosition;
  struct sim_region *simRegion =
      BeginSimRegion(&transientState->transientArena, state, state->world, simRegionOrigin, simBounds, dt);

  RectOutline(renderGroup, v3(0, 0, 0), Rect2GetDim(screenBounds), COLOR_ROSE_600);
  RectOutline(renderGroup, v3(0, 0, 0), RectGetDim(cameraBoundsInMeters).xy, COLOR_ROSE_500);
  RectOutline(renderGroup, v3(0, 0, 0), RectGetDim(simRegion->updatableBounds).xy, COLOR_PINK_500);
  RectOutline(renderGroup, v3(0, 0, 0), RectGetDim(simRegion->bounds).xy, COLOR_PINK_700);

  struct v3 cameraRelativeToSim = WorldPositionSub(world, &state->cameraPosition, &simRegionOrigin);

  for (u32 entityIndex = 0; entityIndex < simRegion->entityCount; entityIndex++) {
    struct entity *entity = simRegion->entities + entityIndex;
    assert(entity);

    if (entity->type == ENTITY_TYPE_INVALID)
      continue;

    if (EntityIsFlagSet(entity, ENTITY_FLAG_NONSPACIAL))
      continue;

    if (!entity->updatable)
      continue;

    // TODO(e2dk4r): probably indicates we want to seperate update and render for entities
    struct v3 cameraRelativeToGround = v3_sub(entity->position, cameraRelativeToSim);
    f32 fadeTopEndZ = 0.75f * state->floorHeight;
    f32 fadeTopStartZ = 0.5f * state->floorHeight;
    f32 fadeBottomStartZ = -2.0f * state->floorHeight;
    f32 fadeBottomEndZ = -2.75f * state->floorHeight;

    renderGroup->alpha = 1.0f;
    if (cameraRelativeToGround.z > fadeTopStartZ) {
      if (cameraRelativeToGround.z <= fadeTopEndZ)
        renderGroup->alpha = 1.0f - Clamp01Range(fadeTopStartZ, fadeTopEndZ, cameraRelativeToGround.z);
      else
        renderGroup->alpha = 0.0f;
    } else if (cameraRelativeToGround.z < fadeBottomStartZ) {
      if (cameraRelativeToGround.z > fadeBottomEndZ)
        renderGroup->alpha = 1.0f - Clamp01Range(fadeBottomStartZ, fadeBottomEndZ, cameraRelativeToGround.z);
      else
        renderGroup->alpha = 0.0f;
    }

    /*****************************************************************
     * Pre-physics entity work
     *****************************************************************/
    if (entity->type & ENTITY_TYPE_HERO) {
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

            PlayAudio(&state->audioState,
                      RandomAudio(&state->generalEntropy, transientState->assets, ASSET_TYPE_BLOOP));
          }
        }
      }
    }

    else if (entity->type & ENTITY_TYPE_FAMILIAR) {
      struct entity *familiar = entity;
      struct entity *closestHero = 0;
      /* 10m maximum search radius */
      f32 closestHeroDistanceSq = Square(10.0f);

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
      if (closestHero && closestHeroDistanceSq > Square(3.0f)) {
        /* there is hero nearby, follow him */
        f32 oneOverLength = 1.0f / SquareRoot(closestHeroDistanceSq);
        ddPosition = v3_mul(v3_sub(closestHero->position, familiar->position), oneOverLength);
      }

      EntityMove(state, simRegion, entity, dt, &FamiliarMoveSpec, ddPosition);

      entity->tBob += dt;
      if (entity->tBob > TAU32)
        entity->tBob -= TAU32;
    }

    else if (entity->type & ENTITY_TYPE_SWORD) {
      struct entity *sword = entity;
      struct v3 oldPosition = sword->position;
      EntityMove(state, simRegion, entity, dt, &SwordMoveSpec, v3(0, 0, 0));

      f32 distanceTraveled = v3_length(v3_sub(sword->position, oldPosition));
      sword->distanceRemaining -= distanceTraveled;
      if (sword->distanceRemaining < 0.0f) {
        EntityAddFlag(sword, ENTITY_FLAG_NONSPACIAL);
        CollisionRuleRemove(state, sword->storageIndex);
      }
    }

    renderGroup->transform.offsetP = entity->position;

    /*****************************************************************
     * Post-physics entity work (rendering)
     *****************************************************************/
    struct hero_bitmap_ids heroBitmapIds = {};
    struct asset_vector matchVector = {};
    matchVector.e[ASSET_TAG_FACING_DIRECTION] = entity->facingDirection;
    struct asset_vector weightVector = {};
    weightVector.e[ASSET_TAG_FACING_DIRECTION] = 1.0f;
    heroBitmapIds.head = BestMatchBitmap(transientState->assets, ASSET_TYPE_HEAD, &matchVector, &weightVector);
    heroBitmapIds.torso = BestMatchBitmap(transientState->assets, ASSET_TYPE_TORSO, &matchVector, &weightVector);
    heroBitmapIds.cape = BestMatchBitmap(transientState->assets, ASSET_TYPE_CAPE, &matchVector, &weightVector);
    if (entity->type & ENTITY_TYPE_HERO) {
      f32 shadowAlpha = 1.0f - entity->position.z;
      if (shadowAlpha < 0.0f)
        shadowAlpha = 0.0f;

      HitPoints(renderGroup, entity);

      f32 heroHeightC = 2.5f;
      f32 heroHeight = heroHeightC * 1.2f;
      BitmapAsset(renderGroup, BitmapGetFirstId(transientState->assets, ASSET_TYPE_SHADOW), v3(0.0f, 0.0f, 0.0f),
                  heroHeightC * 1.0f, v4(1.0f, 1.0f, 1.0f, shadowAlpha));
      BitmapAsset(renderGroup, heroBitmapIds.torso, v3(0.0f, 0.0f, 0.0f), heroHeight, v4(1.0f, 1.0f, 1.0f, 1.0f));
      BitmapAsset(renderGroup, heroBitmapIds.cape, v3(0.0f, 0.0f, 0.0f), heroHeight, v4(1.0f, 1.0f, 1.0f, 1.0f));
      BitmapAsset(renderGroup, heroBitmapIds.head, v3(0.0f, 0.0f, 0.0f), heroHeight, v4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    else if (entity->type & ENTITY_TYPE_FAMILIAR) {
      f32 bobSin = Sin(2.0f * entity->tBob);
      f32 shadowAlpha = (0.5f * 1.0f) - (0.2f * bobSin);

      BitmapAsset(renderGroup, BitmapGetFirstId(transientState->assets, ASSET_TYPE_SHADOW), v3(0.0f, 0.0f, 0.0f), 2.5f,
                  v4(1.0f, 1.0f, 1.0f, shadowAlpha));
      BitmapAsset(renderGroup, heroBitmapIds.head, v3(0.0f, 0.0f, 0.25f * bobSin), 2.5f, v4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    else if (entity->type & ENTITY_TYPE_MONSTER) {
      f32 alpha = 1.0f;

      HitPoints(renderGroup, entity);
      BitmapAsset(renderGroup, BitmapGetFirstId(transientState->assets, ASSET_TYPE_SHADOW), v3(0.0f, 0.0f, 0.0f), 4.5f,
                  v4(1.0f, 1.0f, 1.0f, alpha));
      BitmapAsset(renderGroup, heroBitmapIds.torso, v3(0.0f, 0.0f, 0.0f), 4.5f, v4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    else if (entity->type & ENTITY_TYPE_SWORD) {
      BitmapAsset(renderGroup, BitmapGetFirstId(transientState->assets, ASSET_TYPE_SHADOW), v3(0.0f, 0.0f, 0.0f), 0.5f,
                  v4(1.0f, 1.0f, 1.0f, 1.0f));
      BitmapAsset(renderGroup, BitmapGetFirstId(transientState->assets, ASSET_TYPE_SWORD), v3(0.0f, 0.0f, 0.0f), 0.5f,
                  v4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    else if (entity->type & ENTITY_TYPE_WALL) {
#if 1
      BitmapAsset(renderGroup, BitmapGetFirstId(transientState->assets, ASSET_TYPE_TREE), v3(0.0f, 0.0f, 0.0f), 2.5f,
                  v4(1.0f, 1.0f, 1.0f, 1.0f));
#else
      for (u32 entityVolumeIndex = 0; entity->collision && entityVolumeIndex < entity->collision->volumeCount;
           entityVolumeIndex++) {
        struct entity_collision_volume *entityVolume = entity->collision->volumes + entityVolumeIndex;

        Rect(renderGroup, v3(0.0f, 0.0f, 0.0f), entityVolume->dim.xy, v4(1.0f, 1.0f, 0.0f, 1.0f));
      }
#endif
    }

    else if (entity->type & ENTITY_TYPE_STAIRWELL) {
      Rect(renderGroup, v3(0.0f, 0.0f, 0.0f), entity->walkableDim.xy, v4(1.0f, 0.5f, 0.0f, 1.0f));
      Rect(renderGroup, v3(0.0f, 0.0f, entity->walkableHeight), entity->walkableDim.xy, v4(1.0f, 1.0f, 0.0f, 1.0f));
    }

    else if (entity->type & ENTITY_TYPE_SPACE) {
#if 1
      for (u32 entityVolumeIndex = 0; entity->collision && entityVolumeIndex < entity->collision->volumeCount;
           entityVolumeIndex++) {
        struct entity_collision_volume *entityVolume = entity->collision->volumes + entityVolumeIndex;
        RectOutline(renderGroup, v3_sub(entityVolume->offset, v3(0.0f, 0.0f, entityVolume->dim.z * 0.5f)),
                    entityVolume->dim.xy, v4(0.0f, 0.5f, 1.0f, 1.0f));
      }
#endif
    }

    else {
      assert(0 && "unknown entity type");
    }
  }

#if 0
  // set environment maps
  struct v4 mapColors[] = {
      v4(1.0f, 0.0f, 0.0f, 1.0f),
      v4(0.0f, 1.0f, 0.0f, 1.0f),
      v4(0.0f, 0.0f, 1.0f, 1.0f),
  };

  for (u32 envMapIndex = 0; envMapIndex < ARRAY_COUNT(transientState->envMaps); envMapIndex++) {
    struct environment_map *map = transientState->envMaps + envMapIndex;
    struct bitmap *lod = map->lod + 0;
    u32 checkerWidth = 16;
    u32 checkerHeight = 16;
    u8 rowCheckerOn = 0;
    for (u32 y = 0; y < lod->height; y += checkerHeight) {
      u8 checkerOn = rowCheckerOn;
      for (u32 x = 0; x < lod->width; x += checkerWidth) {
        struct v4 color = checkerOn ? mapColors[envMapIndex] : v4(0.0f, 0.0f, 0.0f, 1.0f);
        struct v2 min = v2u(x, y);
        struct v2 max = v2_add(min, v2u(checkerWidth, checkerHeight));
        DrawRectangle(lod, min, max, color);
        checkerOn = !checkerOn;
      }

      rowCheckerOn = !rowCheckerOn;
    }
  }

  // render scaled rotated textures
  state->time += dt;
  f32 angle = 0.1f * state->time;
  struct v2 disp = {
      100.0f * Cos(5.0f * angle),
      100.0f * Sin(3.0f * angle),
  };

  struct v2 origin = screenCenter;
#if 1
  struct v2 xAxis = v2_mul(v2(Cos(10.0f * angle), Sin(10.0f * angle)), 100.0f);
  struct v2 yAxis = v2_perp(xAxis);
#else
  struct v2 xAxis = v2(100.0f, 0.0f);
  struct v2 yAxis = v2_perp(xAxis);
#endif
  // color angle
  f32 cAngle = 5.0f * angle;
#if 0
  struct v4 color = v4(0.5f + 0.5f * Sin(cAngle), 0.5f + 0.5f * Sin(2.9f * cAngle), 0.5f + 0.5f * Sin(9.9f * cAngle),
                       0.5f + 0.5f * Sin(5.0f * cAngle));
#else
  struct v4 color = v4(1.0f, 1.0f, 1.0f, 1.0f);
#endif
  CoordinateSystem(
      renderGroup, v2_add(v2_add(origin, disp), v2_add(v2_mul(xAxis, -0.5f), v2_mul(yAxis, -0.5f))), xAxis, yAxis,
      color, &state->testDiffuse, &state->testNormal, transientState->envMaps + ENV_MAP_TOP,
      transientState->envMaps + ENV_MAP_MIDDLE, transientState->envMaps + ENV_MAP_BOTTOM);

  // render environment maps
  origin = v2(0.0f, 0.0f);
  for (u32 envMapIndex = 0; envMapIndex < ARRAY_COUNT(transientState->envMaps); envMapIndex++) {
    struct environment_map *map = transientState->envMaps + envMapIndex;
    struct bitmap *lod = map->lod + 0;

    xAxis = v2_mul(v2u(lod->width, 0), 0.5f);
    yAxis = v2_mul(v2u(0, lod->height), 0.5f);
    color = v4(1.0f, 1.0f, 1.0f, 1.0f);

    CoordinateSystem(renderGroup, origin, xAxis, yAxis, color, lod, 0, 0, 0, 0);

    v2_add_ref(&origin, v2_add(yAxis, v2(0.0f, 6.0f)));
  }
#endif

  // Particles system test
  renderGroup->transform.offsetP = v3(0.0f, 0.0f, 0.0f);
  renderGroup->alpha = 1.0f;

  for (u32 particleSpawnIndex = 0; particleSpawnIndex < 1; particleSpawnIndex++) {
    struct particle *particle = state->particles + state->nextParticle;
    state->nextParticle++;

    if (state->nextParticle == ARRAY_COUNT(state->particles))
      state->nextParticle = 0;

    particle->position = v3(0.0f, 0.0f, 0.0f);
    particle->dPosition = v3(0.0f, 1.0f, 0.0f);
    particle->color = v4(1.0f, 1.0f, 1.0f, 2.0f);
    particle->dColor = v4(0.0f, 0.0f, 0.0f, -1.0f);
  }

  for (u32 particleIndex = 0; particleIndex < ARRAY_COUNT(state->particles); particleIndex++) {
    struct particle *particle = state->particles + particleIndex;

    // update
    v3_add_ref(&particle->position, v3_mul(particle->dPosition, dt));

    v4_add_ref(&particle->color, v4_mul(particle->dColor, dt));

    // TODO: should we clamp color in renderer?
    struct v4 color = {
        .r = Clamp01(particle->color.r),
        .g = Clamp01(particle->color.g),
        .b = Clamp01(particle->color.b),
        .a = Clamp01(particle->color.a),
    };

    // render
    BitmapWithColor(renderGroup, &state->testDiffuse, particle->position, 0.3f, color);
  }

  TiledDrawRenderGroup(transientState->highPriorityQueue, renderGroup, &drawBuffer);

  EndSimRegion(simRegion, state);
  EndTemporaryMemory(&simRegionMemory);
  EndTemporaryMemory(&renderMemory);

  MemoryArenaCheck(&state->worldArena);
  MemoryArenaCheck(&transientState->transientArena);

  END_TIMER_BLOCK(GameUpdateAndRender);
}
