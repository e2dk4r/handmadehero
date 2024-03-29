#include <handmadehero/assert.h>
#include <handmadehero/math.h>
#include <handmadehero/world.h>

#define WORLD_CHUNK_SAFE_MARGIN 16
#define WORLD_CHUNK_UNINITIALIZED 0

void WorldInit(struct world *world, f32 tileSideInMeters) {
  world->chunkShift = 4;
  world->chunkDim = (u32)(1 << world->chunkShift);
  world->chunkMask = (u32)(1 << world->chunkShift) - 1;

  world->tileSideInMeters = tileSideInMeters;
}

static inline struct world_chunk *WorldGetChunk(struct world *world, u32 chunkX,
                                                u32 chunkY, u32 chunkZ,
                                                struct memory_arena *arena) {
  assert(chunkX > WORLD_CHUNK_SAFE_MARGIN);
  assert(chunkY > WORLD_CHUNK_SAFE_MARGIN);
  assert(chunkZ > WORLD_CHUNK_SAFE_MARGIN);
  assert(chunkX < U32_MAX - WORLD_CHUNK_SAFE_MARGIN);
  assert(chunkY < U32_MAX - WORLD_CHUNK_SAFE_MARGIN);
  assert(chunkZ < U32_MAX - WORLD_CHUNK_SAFE_MARGIN);

  u32 hashValue = 19 * chunkX + 7 * chunkY + 3 * chunkZ;
  u32 hashSlot = hashValue & (WORLD_CHUNK_TOTAL - 1);
  assert(hashSlot < WORLD_CHUNK_TOTAL);

  struct world_chunk *chunk = &world->worldChunkHash[hashSlot];
  while (1) {
    /* match found */
    if (chunkX == chunk->chunkX && chunkY == chunk->chunkY &&
        chunkZ == chunk->chunkZ)
      break;

    /* if the chunk slot was filled and next is not filled
     * meaning we run out of end of the list
     */
    if (arena && chunk->chunkX != WORLD_CHUNK_UNINITIALIZED && !chunk->next) {
      chunk->next = MemoryArenaPush(arena, sizeof(*chunk));
      chunk = chunk->next;
      chunk->chunkX = 0;
    }

    /* if we are on empty slot */
    if (arena && chunk->chunkX == WORLD_CHUNK_UNINITIALIZED) {
      chunk->chunkX = chunkX;
      chunk->chunkY = chunkY;
      chunk->chunkZ = chunkZ;

      u32 tileCount = world->chunkDim * world->chunkDim;

      chunk->next = 0;
      break;
    }

    chunk = chunk->next;
    if (!chunk)
      break;
  }

  return chunk;
}

static inline void WorldPositionCalculateAxis(struct world *world, u32 *chunk,
                                              f32 *chunkRel) {
  /* NOTE: world is assumed to be toroidal topology, if you step off one end
   * you come back on other.
   */

  i32 offset = roundf32toi32(*chunkRel / world->tileSideInMeters);
  *chunk += (u32)offset;
  *chunkRel -= (f32)offset * world->tileSideInMeters;

  assert(*chunkRel >= -0.5f * world->tileSideInMeters);
  assert(*chunkRel <= 0.5f * world->tileSideInMeters);
}

struct world_position
WorldPositionCalculate(struct world *world, struct world_position *basePosition,
                       struct v2 offset) {
  struct world_position result = *basePosition;
  v2_add_ref(&result.offset, offset);

  WorldPositionCalculateAxis(world, &result.absTileX, &result.offset.x);
  WorldPositionCalculateAxis(world, &result.absTileY, &result.offset.y);

  return result;
}

struct world_difference WorldPositionSub(struct world *world,
                                         struct world_position *a,
                                         struct world_position *b) {
  struct world_difference result = {};

  struct v2 dTileXY = {
      .x = (f32)a->absTileX - (f32)b->absTileX,
      .y = (f32)a->absTileY - (f32)b->absTileY,
  };
  f32 dTileZ = (f32)a->absTileZ - (f32)b->absTileZ;

  result.dXY = v2_add(v2_mul(dTileXY, world->tileSideInMeters),
                      v2_sub(a->offset, b->offset));
  result.dZ = world->tileSideInMeters * dTileZ;

  return result;
}

#if 0
static inline struct world_chunk_position
WorldChunkPositionGet(struct world *world, u32 absTileX, u32 absTileY,
                      u32 absTileZ) {
  struct world_chunk_position result;

  result.tileChunkX = absTileX >> world->chunkShift;
  result.tileChunkY = absTileY >> world->chunkShift;
  result.tileChunkZ = absTileZ;

  result.relTileX = absTileX & world->chunkMask;
  result.relTileY = absTileY & world->chunkMask;

  return result;
}
#endif
