#include <handmadehero/assert.h>
#include <handmadehero/entity.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/sim_region.h>

internal struct entity *
AddEntity(struct game_state *state, struct sim_region *simRegion, u32 storageIndex, struct stored_entity *source,
          struct v3 *simPosition);

internal struct v3
GetPositionRelativeToOrigin(struct sim_region *simRegion, struct stored_entity *stored)
{
  struct v3 diff = WorldPositionSub(simRegion->world, &stored->position, &simRegion->origin);
  return diff;
}

internal u8
IsEntityOverlapsRect(struct rect *rect, struct v3 *dim, struct v3 *position)
{
  struct v3 halfDim = v3_mul(*dim, 0.5f);
  struct rect grown = RectAddRadius(rect, halfDim);
  return RectIsPointInside(&grown, position);
}

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

    struct stored_entity *storedEntity = StoredEntityGet(state, entry->index);
    assert(storedEntity);

    struct v3 positionRelativeToOrigin = GetPositionRelativeToOrigin(simRegion, storedEntity);
    entry->ptr = AddEntity(state, simRegion, ref->index, storedEntity, &positionRelativeToOrigin);
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
          struct v3 *simPosition)
{
  struct entity *dest = AddEntityRaw(state, simRegion, storageIndex, source);
  assert(dest);
  if (simPosition) {
    dest->position = *simPosition;
    dest->updatable = (u8)(IsEntityOverlapsRect(&simRegion->updatableBounds, &dest->dim, &dest->position) & 1);
  } else {
    dest->position = GetPositionRelativeToOrigin(simRegion, source);
  }

  return dest;
}

struct sim_region *
BeginSimRegion(struct memory_arena *simArena, struct game_state *state, struct world *world,
               struct world_position regionCenter, struct rect regionBounds, f32 dt)
{
  struct sim_region *simRegion = MemoryArenaPush(simArena, sizeof(*simRegion));
  ZeroStruct(simRegion->hashTable);

  simRegion->maxEntityRadius = 5.0f;
  simRegion->maxEntityVelocity = 30.0f;
  f32 updateSafetyMargin = simRegion->maxEntityRadius + simRegion->maxEntityVelocity * dt;
  f32 updateSafetyMarginZ = 1.0f;

  simRegion->world = world;
  simRegion->origin = regionCenter;
  simRegion->updatableBounds = RectAddRadius(
      &regionBounds, v3(simRegion->maxEntityRadius, simRegion->maxEntityRadius, simRegion->maxEntityRadius));
  simRegion->bounds =
      RectAddRadius(&simRegion->updatableBounds, v3(updateSafetyMargin, updateSafetyMargin, updateSafetyMarginZ));

  simRegion->entityTotal = 4096;
  simRegion->entityCount = 0;
  simRegion->entities = MemoryArenaPush(simArena, sizeof(*simRegion->entities) * simRegion->entityTotal);

  struct world_position minChunkPosition =
      WorldPositionCalculate(world, &simRegion->origin, RectMin(&simRegion->bounds));
  struct world_position maxChunkPosition =
      WorldPositionCalculate(world, &simRegion->origin, RectMax(&simRegion->bounds));
  for (u32 chunkZ = minChunkPosition.chunkZ; chunkZ <= maxChunkPosition.chunkZ; chunkZ++) {
    for (u32 chunkY = minChunkPosition.chunkY; chunkY <= maxChunkPosition.chunkY; chunkY++) {
      for (u32 chunkX = minChunkPosition.chunkX; chunkX <= maxChunkPosition.chunkX; chunkX++) {
        struct world_chunk *chunk = WorldChunkGet(world, chunkX, chunkY, chunkZ, 0);
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

            struct v3 positionRelativeToOrigin = GetPositionRelativeToOrigin(simRegion, storedEntity);
            if (!IsEntityOverlapsRect(&simRegion->bounds, &entity->dim, &positionRelativeToOrigin))
              continue;

            AddEntity(state, simRegion, storedEntityIndex, storedEntity, &positionRelativeToOrigin);
          }
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
      u32 cameraChunkZ = state->cameraPosition.chunkZ;
      f32 cameraOffsetZ = state->cameraPosition.offset.z;
      struct world_position newCameraPosition = stored->position;
      newCameraPosition.chunkZ = cameraChunkZ;
      newCameraPosition.offset.z = cameraOffsetZ;
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

internal inline u8
ShouldEntitiesOverlap(struct game_state *state, struct entity *moving, struct entity *against)
{
  if (moving == against)
    return 0;

  u8 shouldOverlap = 0;

  if (against->type & ENTITY_TYPE_STAIRWELL)
    shouldOverlap = 1;

  return shouldOverlap;
}

internal void
HandleOverlap(struct game_state *state, struct entity *moving, struct entity *against, f32 *ground)
{
  if (against->type & ENTITY_TYPE_STAIRWELL) {
    struct entity *stairwell = against;
    struct rect stairwellRect = RectCenterDim(stairwell->position, against->dim);
    struct v3 barycentric = v3_clamp01(GetBarycentric(stairwellRect, moving->position));

    *ground = Lerp(stairwellRect.min.z, stairwellRect.max.z, barycentric.y);
  }
}

internal inline u8
ShouldEntitiesCollide(struct game_state *state, struct entity *a, struct entity *b)
{
  if (a == b) {
    return 0;
  }

  u8 shouldCollide = 1;
  if (a->storageIndex > b->storageIndex) {
    struct entity *temp = a;
    a = b;
    b = temp;
  }

  if (a->type & ENTITY_TYPE_STAIRWELL || b->type & ENTITY_TYPE_STAIRWELL) {
    shouldCollide = 0;
  }

  /* if there is any rule about entities, override defaults */
  for (struct pairwise_collision_rule *rule = CollisionRuleGet(state, a->storageIndex); rule; rule = rule->next) {
    if (rule->storageIndexA == a->storageIndex && rule->storageIndexB == b->storageIndex) {
      shouldCollide = rule->shouldCollide;
      break;
    }
  }

  return shouldCollide;
}

internal u8
HandleCollision(struct game_state *state, struct entity *a, struct entity *b)
{
  u8 stopped = 0;

  if (!(a->type & ENTITY_TYPE_SWORD))
    stopped = 1;
  else
    /* collision can only happen once with sword */
    CollisionRuleAdd(state, a->storageIndex, b->storageIndex, 0);

  if (a->type > b->type) {
    struct entity *temp = a;
    a = b;
    b = temp;
  }

  if (a->type & ENTITY_TYPE_MONSTER && b->type & ENTITY_TYPE_SWORD) {
    struct entity *monster = a;
    struct entity *sword = b;

    if (monster->hitPointMax > 0)
      monster->hitPointMax--;
  }

  // TODO: stairs
  // entity->absTileZ += hitEntityLow->dAbsTileZ;

  return stopped;
}

void
EntityMove(struct game_state *state, struct sim_region *simRegion, struct entity *entity, f32 dt,
           const struct move_spec *moveSpec, struct v3 ddPosition)
{
  struct world *world = simRegion->world;

  if (entity->type & ENTITY_TYPE_HERO) {
    u32 breakHere = 1;
  }

  /*****************************************************************
   * CORRECTING ACCELERATION
   *****************************************************************/
  if (moveSpec->unitMaxAccel) {
    f32 ddPositionLength = v3_length_square(ddPosition);
    /*
     * scale down acceleration to unit vector
     * fixes moving diagonally √2 times faster
     */
    if (ddPositionLength > 1.0f) {
      v3_mul_ref(&ddPosition, 1 / SquareRoot(ddPositionLength));
    }
  }

  /* set entity speed in m/s² */
  v3_mul_ref(&ddPosition, moveSpec->speed);

  /* apply friction opposite force to acceleration */
  v2_add_ref(&ddPosition.xy, v2_neg(v2_mul(entity->dPosition.xy, moveSpec->drag)));

  /* apply gravity */
  const f32 earthSurfaceGravity = 9.80665f; /* m/s² */
  ddPosition.z += -earthSurfaceGravity;

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
  struct v3 deltaPosition =
      /* 1/2 a t² + v t */
      v3_add(
          /* 1/2 a t² */
          v3_mul(
              /* 1/2 a */
              v3_mul(ddPosition, 0.5f),
              /* t² */
              square(dt)), /* end: 1/2 a t² */
          /* v t */
          v3_mul(
              /* v */
              entity->dPosition,
              /* t */
              dt) /* end: v t */
      );

  /* new velocity */
  /* v' = a t + v */
  v3_add_ref(&entity->dPosition,
             /* a t */
             v3_mul(
                 /* a */
                 ddPosition,
                 /* t */
                 dt));

  // TODO(e2dk4r): upgrade physical motion routines to handle capping the maximum velocity
  assert(v3_length_square(entity->dPosition) <= square(simRegion->maxEntityVelocity));

  /*****************************************************************
   * COLLUSION DETECTION
   *****************************************************************/
  for (u32 iteration = 0; iteration < 4; iteration++) {
    f32 tMin = 1.0f;
    struct v3 wallNormal;
    struct v3 desiredPosition = v3_add(entity->position, deltaPosition);
    struct entity *hitEntity = 0;

    for (u32 testEntityIndex = 0; testEntityIndex < simRegion->entityCount; testEntityIndex++) {
      struct entity *testEntity = simRegion->entities + testEntityIndex;

      if (!ShouldEntitiesCollide(state, entity, testEntity))
        continue;

      if (entity->position.z != testEntity->position.z)
        continue;

      struct v3 minkowskiDiameter = v3_add(entity->dim, testEntity->dim);
      struct v3 minCorner = v3_mul(minkowskiDiameter, -0.5f);
      struct v3 maxCorner = v3_mul(minkowskiDiameter, 0.5f);

      struct v3 rel = v3_sub(entity->position, testEntity->position);

      /* test all 4 walls and take minimum t. */
      if (WallTest(&tMin, minCorner.x, rel.x, rel.y, deltaPosition.x, deltaPosition.y, minCorner.y, maxCorner.y)) {
        wallNormal = v3(-1, 0, 0);
        hitEntity = testEntity;
      }

      if (WallTest(&tMin, maxCorner.x, rel.x, rel.y, deltaPosition.x, deltaPosition.y, minCorner.y, maxCorner.y)) {
        wallNormal = v3(1, 0, 0);
        hitEntity = testEntity;
      }

      if (WallTest(&tMin, minCorner.y, rel.y, rel.x, deltaPosition.y, deltaPosition.x, minCorner.x, maxCorner.x)) {
        wallNormal = v3(0, -1, 0);
        hitEntity = testEntity;
      }

      if (WallTest(&tMin, maxCorner.y, rel.y, rel.x, deltaPosition.y, deltaPosition.x, minCorner.x, maxCorner.x)) {
        wallNormal = v3(0, 1, 0);
        hitEntity = testEntity;
      }
    }

    /* p' = tMin (1/2 a t² + v t) + p */
    v3_add_ref(&entity->position, v3_mul(deltaPosition, tMin));

    if (!hitEntity)
      break;

    /*****************************************************************
     * COLLUSION HANDLING
     *****************************************************************/
    deltaPosition = v3_sub(desiredPosition, entity->position);

    u8 stopsOnCollision = HandleCollision(state, entity, hitEntity);
    if (stopsOnCollision) {
      /*
       * add gliding to velocity
       */
      // clang-format off
      /* v' = v - vTr r */
      v3_sub_ref(&entity->dPosition,
          /* vTr r */
          v3_mul(
            /* r */
            wallNormal,
            /* vTr */
            v3_dot(entity->dPosition, wallNormal)
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
      v3_sub_ref(&deltaPosition,
          /* pTr r */
          v3_mul(
            /* r */
            wallNormal,
            /* pTr */
            v3_dot(deltaPosition, wallNormal)
          )
      );

      // clang-format on
    }
  }

  // NOTE: Handle events based on area overlapping
  f32 ground = 0.0f;
  struct rect entityRect = RectCenterDim(entity->position, entity->dim);
  for (u32 testEntityIndex = 0; testEntityIndex < simRegion->entityCount; testEntityIndex++) {
    struct entity *testEntity = simRegion->entities + testEntityIndex;

    if (!ShouldEntitiesOverlap(state, entity, testEntity))
      continue;

    struct rect testEntityRect = RectCenterDim(testEntity->position, testEntity->dim);
    if (IsRectIntersect(&entityRect, &testEntityRect)) {
      HandleOverlap(state, entity, testEntity, &ground);
    }
  }

  // TODO(e2dk4r): this has to become real high handling / ground collision / etc.
  if (entity->position.z < ground) {
    entity->position.z = ground;
    entity->dPosition.z = 0;
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
