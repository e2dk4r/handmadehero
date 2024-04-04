#include <handmadehero/assert.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/sim_region.h>

internal struct sim_entity *
AddEntityRaw(struct sim_region *simRegion)
{
  struct sim_entity *entity = 0;

  if (simRegion->entityCount >= simRegion->entityTotal)
    InvalidCodePath;

  u32 entityIndex = simRegion->entityCount;
  entity = simRegion->entities + entityIndex;
  *entity = (struct sim_entity){};
  simRegion->entityCount++;

  return entity;
}

internal struct v2
GetPositionRelativeToOrigin(struct sim_region *simRegion, struct entity_low *stored)
{
  struct world_difference diff = WorldPositionSub(simRegion->world, &stored->position, &simRegion->origin);
  struct v2 result = diff.dXY;
  return result;
}

internal struct sim_entity *
AddEntity(struct sim_region *simRegion, struct entity_low *source, struct v2 *simPosition)
{
  struct sim_entity *dest = AddEntityRaw(simRegion);
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
          struct entity_low *storedEntity = state->storedEntities + storedEntityIndex;
          assert(storedEntity);

          if (storedEntity->type == ENTITY_TYPE_INVALID)
            continue;

          struct v2 positionRelativeToOrigin = GetPositionRelativeToOrigin(simRegion, storedEntity);
          if (!RectIsPointInside(simRegion->bounds, positionRelativeToOrigin))
            continue;

          AddEntity(simRegion, storedEntity, &positionRelativeToOrigin);
        }
      }
    }
  }

  return simRegion;
}

void
EndSimRegion(struct sim_region *region, struct game_state *state)
{
  struct sim_entity *entity = region->entities;
  for (u32 entityIndex = 1; entityIndex < region->entityCount; entityIndex++, entity++) {
    struct entity_low *stored = state->storedEntities + entity->storageIndex;
    assert(stored);

    struct world_position newPosition = WorldPositionCalculate(state->world, &state->cameraPos, entity->position);
    EntityChangeLocation(&state->worldArena, state->world, entity->storageIndex, stored, &stored->position,
                         &newPosition);
  }

  /* sync camera with followed entity */
  struct entity_low *followedEntityLow = state->storedEntities + state->followedEntityIndex;
  if (followedEntityLow) {
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
}
