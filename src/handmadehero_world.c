#include <handmadehero/assert.h>
#include <handmadehero/math.h>
#include <handmadehero/world.h>

#define WORLD_CHUNK_SAFE_MARGIN 16
#define WORLD_CHUNK_UNINITIALIZED 0
#define TILES_PER_CHUNK 16

void WorldInit(struct world *world, f32 tileSideInMeters) {
  world->tileSideInMeters = tileSideInMeters;
  world->chunkSideInMeters = (f32)TILES_PER_CHUNK * tileSideInMeters;
  world->firstFreeBlock = 0;

  for (u32 chunkIndex = 0; chunkIndex < WORLD_CHUNK_TOTAL; chunkIndex++) {
    world->chunkHash[chunkIndex].chunkX = WORLD_CHUNK_UNINITIALIZED;
    world->chunkHash[chunkIndex].firstBlock.entityCount = 0;
  }
}

inline struct world_chunk *WorldChunkGet(struct world *world, u32 chunkX,
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

  struct world_chunk *chunk = &world->chunkHash[hashSlot];
  while (chunk) {
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

      chunk->next = 0;
      break;
    }

    chunk = chunk->next;
  }

  return chunk;
}

static inline u8 WorldPositionIsCalculated(struct world *world, f32 chunkRel) {
  return (chunkRel >= -0.5f * world->chunkSideInMeters) &&
         (chunkRel <= 0.5f * world->chunkSideInMeters);
}

static inline u8 WorldPositionIsCalculatedOffset(struct world *world,
                                                 struct v2 *offset) {

  return WorldPositionIsCalculated(world, offset->x) &&
         WorldPositionIsCalculated(world, offset->y);
}

static inline void WorldPositionCalculateAxis(struct world *world, u32 *chunk,
                                              f32 *chunkRel) {
  /* NOTE: world is assumed to be toroidal topology, if you step off one end
   * you come back on other.
   */

  i32 offset = roundf32toi32(*chunkRel / world->chunkSideInMeters);
  *chunk += (u32)offset;
  *chunkRel -= (f32)offset * world->chunkSideInMeters;

  assert(WorldPositionIsCalculated(world, *chunkRel));
}

struct world_position
WorldPositionCalculate(struct world *world, struct world_position *basePosition,
                       struct v2 offset) {
  struct world_position result = *basePosition;
  v2_add_ref(&result.offset, offset);

  WorldPositionCalculateAxis(world, &result.chunkX, &result.offset.x);
  WorldPositionCalculateAxis(world, &result.chunkY, &result.offset.y);

  return result;
}

inline struct world_position ChunkPositionFromTilePosition(struct world *world,
                                                           u32 absTileX,
                                                           u32 absTileY,
                                                           u32 absTileZ) {
  struct world_position result = {};

  // TODO: move to z!!

  result.chunkX = absTileX / TILES_PER_CHUNK;
  result.chunkY = absTileY / TILES_PER_CHUNK;
  result.chunkZ = absTileZ / TILES_PER_CHUNK;

  result.offset.x = (f32)(absTileX - (result.chunkX * TILES_PER_CHUNK)) *
                        world->tileSideInMeters -
                    world->chunkSideInMeters / 2;
  result.offset.y = (f32)(absTileY - (result.chunkY * TILES_PER_CHUNK)) *
                        world->tileSideInMeters -
                    world->chunkSideInMeters / 2;

  assert(WorldPositionIsCalculatedOffset(world, &result.offset));

  return result;
}

struct world_difference WorldPositionSub(struct world *world,
                                         struct world_position *a,
                                         struct world_position *b) {
  struct world_difference result = {};

  struct v2 dTileXY =
      v2((f32)(a->chunkX - b->chunkX), (f32)(a->chunkY - b->chunkY));
  f32 dTileZ = (f32)(a->chunkZ - b->chunkZ);

  /* overflowed check */
  if (a->chunkX < b->chunkX)
    dTileXY.x = (f32)(b->chunkX - a->chunkX) * -1;
  if (a->chunkY < b->chunkY)
    dTileXY.y = (f32)(b->chunkY - a->chunkY) * -1;
  if (a->chunkZ < b->chunkZ)
    dTileZ = (f32)(b->chunkZ - a->chunkZ) * -1;

  result.dXY = v2_add(v2_mul(dTileXY, world->chunkSideInMeters),
                      v2_sub(a->offset, b->offset));
  result.dZ = world->chunkSideInMeters * dTileZ;

  return result;
}

static inline u8 WorldPositionSame(struct world *world,
                                   struct world_position *left,
                                   struct world_position *right) {
  assert(WorldPositionIsCalculatedOffset(world, &left->offset));
  assert(WorldPositionIsCalculatedOffset(world, &right->offset));
  return
      /* x */
      left->chunkX == right->chunkX
      /* y */
      && left->chunkY == right->chunkY
      /* z */
      && left->chunkZ == right->chunkZ;
}

inline void EntityChangeLocation(struct memory_arena *arena,
                                 struct world *world, u32 entityLowIndex,
                                 struct world_position *oldPosition,
                                 struct world_position *newPosition) {
  assert(newPosition);
  if (oldPosition && WorldPositionSame(world, oldPosition, newPosition))
    // leave entity where it is
    return;

  if (oldPosition) {
    // pull entity out of its old entity block
    struct world_chunk *chunk =
        WorldChunkGet(world, oldPosition->chunkX, oldPosition->chunkY,
                      oldPosition->chunkZ, 0);
    assert(chunk);
    struct world_entity_block *firstBlock = &chunk->firstBlock;
    u8 found = 0;
    for (struct world_entity_block *block = firstBlock; !found && block;
         block = block->next) {
      for (u32 blockEntityIndex = 0;
           !found && blockEntityIndex < block->entityCount;
           blockEntityIndex++) {

        if (block->entityLowIndexes[blockEntityIndex] != entityLowIndex)
          continue;

        u32 firstBlockEntityLastIndex = firstBlock->entityCount - 1;
        block->entityLowIndexes[blockEntityIndex] =
            firstBlock->entityLowIndexes[firstBlockEntityLastIndex];
        firstBlock->entityCount--;

        if (firstBlock->entityCount == 0) {
          if (firstBlock->next) {
            struct world_entity_block *nextBlock = firstBlock->next;
            *firstBlock = *firstBlock->next;

            nextBlock->next = world->firstFreeBlock;
            world->firstFreeBlock = nextBlock;
          }

          found = 1;
          break;
        }

        found = 1;
      }
    }
  }

  // insert into its new entity block
  struct world_chunk *chunk =
      WorldChunkGet(world, newPosition->chunkX, newPosition->chunkY,
                    newPosition->chunkZ, arena);
  struct world_entity_block *block = &chunk->firstBlock;
  if (block->entityCount == ARRAY_COUNT(block->entityLowIndexes)) {
    // we're out of room, get a new block
    struct world_entity_block *oldBlock = world->firstFreeBlock;

    if (oldBlock) {
      world->firstFreeBlock = oldBlock->next;
    } else {
      oldBlock = MemoryArenaPush(arena, sizeof(*oldBlock));
    }

    *oldBlock = *block;
    block->next = oldBlock;
    block->entityCount = 0;
  }

  assert(block->entityCount < ARRAY_COUNT(block->entityLowIndexes));
  block->entityLowIndexes[block->entityCount] = entityLowIndex;
  block->entityCount++;
}
