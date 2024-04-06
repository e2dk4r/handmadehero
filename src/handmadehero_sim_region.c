#include <handmadehero/assert.h>
#include <handmadehero/entity.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/sim_region.h>

internal struct entity *
AddEntity(struct game_state *state, struct sim_region *simRegion, u32 storageIndex, struct stored_entity *source,
          struct v2 *simPosition);

internal struct sim_entity_hash *
GetHashFromStorageIndex(struct sim_region *simRegion, u32 storageIndex)
{
  assert(storageIndex);

  struct sim_entity_hash *result = 0;
  u32 hashValue = storageIndex;

  for (u32 offset = 0; offset < ARRAY_COUNT(simRegion->hashTable); offset++) {
    u32 hashMask = ARRAY_COUNT(simRegion->hashTable) - 1;
    u32 hashIndex = (hashValue + offset) & hashMask;
    struct sim_entity_hash *entry = simRegion->hashTable + hashIndex;

    if (entry->index == 0 || entry->index == storageIndex) {
      result = entry;
      break;
    }
  }

  return result;
}

internal struct entity *
GetEntityFromStorageIndex(struct sim_region *simRegion, u32 storageIndex)
{
  struct sim_entity_hash *entry = GetHashFromStorageIndex(simRegion, storageIndex);
  if (!entry)
    return 0;
  struct entity *result = entry->ptr;
  return result;
}

internal void
MapStorageIndexToEntity(struct sim_region *simRegion, u32 storageIndex, struct entity *entity)
{
  struct sim_entity_hash *entry = GetHashFromStorageIndex(simRegion, storageIndex);
  assert(entry->index == 0 || entry->index == storageIndex);
  entry->index = storageIndex;
  entry->ptr = entity;
}

internal inline void
LoadEntityReference(struct game_state *state, struct sim_region *simRegion, struct entity_reference *ref)
{
  if (!ref->index)
    return;

  struct sim_entity_hash *entry = GetHashFromStorageIndex(simRegion, ref->index);
  if (entry->ptr == 0) {
    entry->index = ref->index;
    entry->ptr = AddEntity(state, simRegion, ref->index, StoredEntityGet(state, ref->index), 0);
  }

  ref->ptr = entry->ptr;
}

internal inline void
StoreEntityReference(struct entity_reference *ref)
{
  if (!ref->ptr)
    return;

  ref->index = ref->ptr->storageIndex;
}

internal struct v2
GetPositionRelativeToOrigin(struct sim_region *simRegion, struct stored_entity *stored)
{
  struct world_difference diff = WorldPositionSub(simRegion->world, &stored->position, &simRegion->origin);
  struct v2 result = diff.dXY;
  return result;
}

internal struct entity *
AddEntityRaw(struct game_state *state, struct sim_region *simRegion, u32 storageIndex, struct stored_entity *source)
{
  struct entity *entity = 0;

  if (simRegion->entityCount >= simRegion->entityTotal)
    InvalidCodePath;

  u32 entityIndex = simRegion->entityCount;
  entity = simRegion->entities + entityIndex;
  simRegion->entityCount++;
  MapStorageIndexToEntity(simRegion, storageIndex, entity);

  if (source) {
    *entity = source->sim;
    LoadEntityReference(state, simRegion, &entity->sword);
  }

  entity->storageIndex = storageIndex;

  return entity;
}

internal struct entity *
AddEntity(struct game_state *state, struct sim_region *simRegion, u32 storageIndex, struct stored_entity *source,
          struct v2 *simPosition)
{
  struct entity *dest = AddEntityRaw(state, simRegion, storageIndex, source);
  assert(dest);
  if (simPosition) {
    dest->position = *simPosition;
  } else {
    dest->position = GetPositionRelativeToOrigin(simRegion, source);
  }

  return dest;
}

struct sim_region *
BeginSimRegion(struct memory_arena *simArena, struct game_state *state, struct world *world,
               struct world_position regionCenter, struct rectangle2 regionBounds)
{
  struct sim_region *simRegion = MemoryArenaPush(simArena, sizeof(*simRegion));
  ZeroStruct(simRegion->hashTable);

  simRegion->world = world;
  simRegion->origin = regionCenter;
  simRegion->bounds = regionBounds;

  simRegion->entityTotal = 4096;
  simRegion->entityCount = 0;
  simRegion->entities = MemoryArenaPush(simArena, sizeof(*simRegion->entities) * simRegion->entityTotal);

  struct world_position minChunkPosition =
      WorldPositionCalculate(world, &simRegion->origin, RectMin(&simRegion->bounds));
  struct world_position maxChunkPosition =
      WorldPositionCalculate(world, &simRegion->origin, RectMax(&simRegion->bounds));
  for (u32 chunkX = minChunkPosition.chunkX; chunkX <= maxChunkPosition.chunkX; chunkX++) {
    for (u32 chunkY = minChunkPosition.chunkY; chunkY <= maxChunkPosition.chunkY; chunkY++) {
      struct world_chunk *chunk = WorldChunkGet(world, chunkX, chunkY, simRegion->origin.chunkZ, 0);
      if (!chunk)
        continue;

      for (struct world_entity_block *block = &chunk->firstBlock; block; block = block->next) {
        for (u32 entityIndex = 0; entityIndex < block->entityCount; entityIndex++) {
          u32 storedEntityIndex = block->entityLowIndexes[entityIndex];
          struct stored_entity *storedEntity = state->storedEntities + storedEntityIndex;
          assert(storedEntity);
          struct entity *entity = &storedEntity->sim;

          if (entity->type == ENTITY_TYPE_INVALID)
            continue;
          if (EntityIsFlagSet(entity, ENTITY_FLAG_NONSPACIAL))
            continue;

          struct v2 positionRelativeToOrigin = GetPositionRelativeToOrigin(simRegion, storedEntity);
          if (!RectIsPointInside(simRegion->bounds, positionRelativeToOrigin))
            continue;

          AddEntity(state, simRegion, storedEntityIndex, storedEntity, &positionRelativeToOrigin);
        }
      }
    }
  }

  return simRegion;
}

void
EndSimRegion(struct sim_region *simRegion, struct game_state *state)
{
  for (u32 entityIndex = 0; entityIndex < simRegion->entityCount; entityIndex++) {
    struct entity *entity = simRegion->entities + entityIndex;
    struct stored_entity *stored = state->storedEntities + entity->storageIndex;
    assert(stored);

    stored->sim = *entity;
    StoreEntityReference(&stored->sim.sword);

    struct world_position *newPosition = 0;
    if (!EntityIsFlagSet(entity, ENTITY_FLAG_NONSPACIAL)) {
      struct world_position relativePositionFromOrigin =
          WorldPositionCalculate(state->world, &simRegion->origin, entity->position);
      newPosition = &relativePositionFromOrigin;
    }

    EntityChangeLocation(&state->worldArena, state->world, entity->storageIndex, stored, &stored->position,
                         newPosition);

    /* sync camera with followed entity */
    if (entity->storageIndex == state->followedEntityIndex) {
#if 0
      struct world_position newCameraPosition = state->cameraPosition;
      newCameraPosition.chunkZ = stored->position.chunkZ;

      const u32 scrollWidth = 17;
      const u32 scrollHeight = 9;

      f32 maxDiffX = (f32)scrollWidth * 0.5f * simRegion->world->tileSideInMeters;
      if (entity->position.x > maxDiffX)
        newCameraPosition.chunkX += 1;
      else if (entity->position.x < -maxDiffX)
        newCameraPosition.chunkX -= 1;

      f32 maxDiffY = (f32)scrollHeight * 0.5f * simRegion->world->tileSideInMeters;
      if (entity->position.y > maxDiffY)
        newCameraPosition.chunkY += 1;
      else if (entity->position.y < -maxDiffY)
        newCameraPosition.chunkY -= 1;
#else
      struct world_position newCameraPosition = stored->position;
#endif
      state->cameraPosition = newCameraPosition;
    }
  }
}

internal u8
WallTest(f32 *tMin, f32 wallX, f32 relX, f32 relY, f32 deltaX, f32 deltaY, f32 minY, f32 maxY)
{
  const f32 tEpsilon = 0.001f;
  u8 collided = 0;

  /* no movement, no sweet */
  if (deltaX == 0)
    return collided;

  f32 tResult = (wallX - relX) / deltaX;
  if (tResult < 0)
    return collided;
  /* do not care, if entity needs to go back to hit */
  if (tResult >= *tMin)
    return collided;

  f32 y = relY + tResult * deltaY;
  if (y < minY)
    return collided;
  if (y > maxY)
    return collided;

  *tMin = maximum(0.0f, tResult - tEpsilon);
  collided = 1;

  return collided;
}

void
EntityMove(struct sim_region *simRegion, struct entity *entity, f32 dt, const struct move_spec *moveSpec,
           struct v2 ddPosition)
{
  struct world *world = simRegion->world;

  /*****************************************************************
   * CORRECTING ACCELERATION
   *****************************************************************/
  if (moveSpec->unitMaxAccel) {
    f32 ddPositionLength = v2_length_square(ddPosition);
    /*
     * scale down acceleration to unit vector
     * fixes moving diagonally √2 times faster
     */
    if (ddPositionLength > 1.0f) {
      v2_mul_ref(&ddPosition, 1 / SquareRoot(ddPositionLength));
    }
  }

  /* set entity speed in m/s² */
  v2_mul_ref(&ddPosition, moveSpec->speed);

  /*
   * apply friction opposite force to acceleration
   */
  v2_add_ref(&ddPosition, v2_neg(v2_mul(entity->dPosition, moveSpec->drag)));

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
          entity->dPosition,
          /* t */
          dt
        ) /* end: v t */
      );
  // clang-format on

  /* new velocity */
  // clang-format off
  /* v' = a t + v */
  v2_add_ref(&entity->dPosition,
    /* a t */
    v2_mul(
      /* a */
      ddPosition,
      /* t */
      dt
    )
  );
  // clang-format on

  /*****************************************************************
   * COLLUSION DETECTION
   *****************************************************************/
  for (u32 iteration = 0; iteration < 4; iteration++) {
    f32 tMin = 1.0f;
    struct v2 wallNormal;
    struct v2 desiredPosition = v2_add(entity->position, deltaPosition);
    struct entity *hitEntity = 0;

    if (EntityIsFlagSet(entity, ENTITY_FLAG_COLLIDE)) {
      for (u32 testEntityIndex = 0; testEntityIndex < simRegion->entityCount; testEntityIndex++) {
        struct entity *testEntity = simRegion->entities + testEntityIndex;

        if (entity == testEntity || !EntityIsFlagSet(testEntity, ENTITY_FLAG_COLLIDE))
          continue;

        struct v2 diameter = {
            .x = testEntity->width + entity->width,
            .y = testEntity->height + entity->height,
        };

        struct v2 minCorner = v2_mul(diameter, -0.5f);
        struct v2 maxCorner = v2_mul(diameter, 0.5f);

        struct v2 rel = v2_sub(entity->position, testEntity->position);

        /* test all 4 walls and take minimum t. */
        if (WallTest(&tMin, minCorner.x, rel.x, rel.y, deltaPosition.x, deltaPosition.y, minCorner.y, maxCorner.y)) {
          wallNormal = v2(-1, 0);
          hitEntity = testEntity;
        }

        if (WallTest(&tMin, maxCorner.x, rel.x, rel.y, deltaPosition.x, deltaPosition.y, minCorner.y, maxCorner.y)) {
          wallNormal = v2(1, 0);
          hitEntity = testEntity;
        }

        if (WallTest(&tMin, minCorner.y, rel.y, rel.x, deltaPosition.y, deltaPosition.x, minCorner.x, maxCorner.x)) {
          wallNormal = v2(0, -1);
          hitEntity = testEntity;
        }

        if (WallTest(&tMin, maxCorner.y, rel.y, rel.x, deltaPosition.y, deltaPosition.x, minCorner.x, maxCorner.x)) {
          wallNormal = v2(0, 1);
          hitEntity = testEntity;
        }
      }
    }

    /* p' = tMin (1/2 a t² + v t) + p */
    v2_add_ref(&entity->position, v2_mul(deltaPosition, tMin));

    /*****************************************************************
     * COLLUSION HANDLING
     *****************************************************************/
    if (hitEntity) {
      /*
       * add gliding to velocity
       */
      // clang-format off
      /* v' = v - vTr r */
      v2_sub_ref(&entity->dPosition,
          /* vTr r */
          v2_mul(
            /* r */
            wallNormal,
            /* vTr */
            v2_dot(entity->dPosition, wallNormal)
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
      deltaPosition = v2_sub(desiredPosition, entity->position);
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
      // entity->absTileZ += hitEntityLow->dAbsTileZ;
    } else {
      break;
    }
  }

  /*****************************************************************
   * VISUAL CORRECTIONS
   *****************************************************************/
  /* use player velocity to face the direction */

  if (entity->dPosition.x == 0.0f && entity->dPosition.y == 0.0f)
    ;
  else if (absolute(entity->dPosition.x) > absolute(entity->dPosition.y)) {
    if (entity->dPosition.x < 0)
      entity->facingDirection = BITMAP_HERO_LEFT;
    else
      entity->facingDirection = BITMAP_HERO_RIGHT;
  } else {
    if (entity->dPosition.y > 0)
      entity->facingDirection = BITMAP_HERO_BACK;
    else
      entity->facingDirection = BITMAP_HERO_FRONT;
  }
}
