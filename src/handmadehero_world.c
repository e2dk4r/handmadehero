#include <handmadehero/assert.h>
#include <handmadehero/entity.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/math.h>
#include <handmadehero/world.h>

#define WORLD_CHUNK_SAFE_MARGIN 16
#define WORLD_CHUNK_UNINITIALIZED 0
#define TILES_PER_CHUNK 8

void
WorldInit(struct world *world, f32 tileSideInMeters)
{
  world->tileSideInMeters = tileSideInMeters;
  world->tileDepthInMeters = tileSideInMeters;
  world->chunkDimInMeters = v3((f32)TILES_PER_CHUNK * world->tileSideInMeters,
                               (f32)TILES_PER_CHUNK * world->tileSideInMeters, world->tileDepthInMeters);
  world->firstFreeBlock = 0;

  for (u32 chunkIndex = 0; chunkIndex < WORLD_CHUNK_TOTAL; chunkIndex++) {
    world->chunkHash[chunkIndex].chunkX = WORLD_CHUNK_UNINITIALIZED;
    world->chunkHash[chunkIndex].firstBlock.entityCount = 0;
  }
}

internal inline struct world_chunk *
WorldChunkGetOrInsert(struct world *world, u32 chunkX, u32 chunkY, u32 chunkZ, struct memory_arena *arena)
{
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
    if (chunkX == chunk->chunkX && chunkY == chunk->chunkY && chunkZ == chunk->chunkZ)
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

inline struct world_chunk *
WorldChunkGet(struct world *world, u32 chunkX, u32 chunkY, u32 chunkZ)
{
  return WorldChunkGetOrInsert(world, chunkX, chunkY, chunkZ, 0);
}

internal inline u8
WorldPositionIsCalculated(f32 chunkDim, f32 chunkRel)
{
  const f32 epsilon = 0.0001f;
  f32 max = 0.5f * chunkDim + epsilon;
  f32 min = -max;
  return (chunkRel >= min) && (chunkRel <= max);
}

internal inline u8
WorldPositionIsCalculatedOffset(struct world *world, struct v3 *offset)
{

  return WorldPositionIsCalculated(world->chunkDimInMeters.x, offset->x) &&
         WorldPositionIsCalculated(world->chunkDimInMeters.y, offset->y) &&
         WorldPositionIsCalculated(world->chunkDimInMeters.z, offset->z);
}

internal inline void
WorldPositionCalculateAxis(f32 chunkDim, u32 *chunk, f32 *chunkRel)
{
  /* NOTE: world is assumed to be toroidal topology, if you step off one end
   * you come back on other.
   */

  i32 offset = roundf32toi32(*chunkRel / chunkDim);
  *chunk += (u32)offset;
  *chunkRel -= (f32)offset * chunkDim;

  assert(WorldPositionIsCalculated(chunkDim, *chunkRel));
}

struct world_position
WorldPositionCalculate(struct world *world, struct world_position *basePosition, struct v3 offset)
{
  struct world_position result = *basePosition;
  v3_add_ref(&result.offset, offset);

  WorldPositionCalculateAxis(world->chunkDimInMeters.x, &result.chunkX, &result.offset.x);
  WorldPositionCalculateAxis(world->chunkDimInMeters.y, &result.chunkY, &result.offset.y);
  WorldPositionCalculateAxis(world->chunkDimInMeters.z, &result.chunkZ, &result.offset.z);

  return result;
}

inline struct world_position
ChunkPositionFromTilePosition(struct world *world, u32 absTileX, u32 absTileY, u32 absTileZ)
{
  struct world_position result = {};

  /* calculating using hadamard product considered, but it happens to cause float overflow
   * with U32_MAX
   *
   *   basePosition = 0
   *   WorldPositionCalculate(basePosition, hadamard(chunkDimInMeters, v3(x, y, z))
   *
   */
  result.chunkX = absTileX / TILES_PER_CHUNK;
  result.chunkY = absTileY / TILES_PER_CHUNK;
  result.chunkZ = absTileZ;

  result.offset.x =
      (f32)(absTileX - (result.chunkX * TILES_PER_CHUNK)) * world->tileSideInMeters - world->chunkDimInMeters.x / 2;
  result.offset.y =
      (f32)(absTileY - (result.chunkY * TILES_PER_CHUNK)) * world->tileSideInMeters - world->chunkDimInMeters.y / 2;
  result.offset.z = 0;

  assert(WorldPositionIsCalculatedOffset(world, &result.offset));
  return result;
}

struct v3
WorldPositionSub(struct world *world, struct world_position *a, struct world_position *b)
{
  struct v3 dTile = (struct v3){/* x */
                                (f32)(a->chunkX - b->chunkX),
                                /* y */
                                (f32)(a->chunkY - b->chunkY),
                                /* z */
                                (f32)(a->chunkZ - b->chunkZ)};

  /* overflowed check */
  if (a->chunkX < b->chunkX)
    dTile.x = (f32)(b->chunkX - a->chunkX) * -1;
  if (a->chunkY < b->chunkY)
    dTile.y = (f32)(b->chunkY - a->chunkY) * -1;
  if (a->chunkZ < b->chunkZ)
    dTile.z = (f32)(b->chunkZ - a->chunkZ) * -1;

  struct v3 result = v3_add(v3_hadamard(world->chunkDimInMeters, dTile), v3_sub(a->offset, b->offset));
  return result;
}

internal inline u8
WorldPositionSame(struct world *world, struct world_position *left, struct world_position *right)
{
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

internal inline void
EntityChangeLocationRaw(struct memory_arena *arena, struct world *world, u32 entityLowIndex,
                        struct world_position *oldPosition, struct world_position *newPosition)
{
  assert(!oldPosition || WorldPositionIsValid(oldPosition));
  assert(!newPosition || WorldPositionIsValid(newPosition));
  if (oldPosition && newPosition && WorldPositionSame(world, oldPosition, newPosition))
    // leave entity where it is
    return;

  if (oldPosition) {
    // pull entity out of its old entity block
    struct world_chunk *chunk = WorldChunkGet(world, oldPosition->chunkX, oldPosition->chunkY, oldPosition->chunkZ);
    assert(chunk);
    struct world_entity_block *firstBlock = &chunk->firstBlock;
    u8 found = 0;
    for (struct world_entity_block *block = firstBlock; !found && block; block = block->next) {
      for (u32 blockEntityIndex = 0; !found && blockEntityIndex < block->entityCount; blockEntityIndex++) {

        if (block->entityLowIndexes[blockEntityIndex] != entityLowIndex)
          continue;

        u32 firstBlockEntityLastIndex = firstBlock->entityCount - 1;
        block->entityLowIndexes[blockEntityIndex] = firstBlock->entityLowIndexes[firstBlockEntityLastIndex];
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
      WorldChunkGetOrInsert(world, newPosition->chunkX, newPosition->chunkY, newPosition->chunkZ, arena);
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

inline void
EntityChangeLocation(struct memory_arena *arena, struct world *world, u32 entityLowIndex, struct stored_entity *stored,
                     struct world_position *oldPosition, struct world_position *newPosition)
{
  struct entity *entity = &stored->sim;
  if (newPosition) {
    stored->position = *newPosition;
    EntityChangeLocationRaw(arena, world, entityLowIndex, oldPosition, newPosition);
    EntityClearFlag(entity, ENTITY_FLAG_NONSPACIAL);
  } else {
    stored->position = WorldPositionInvalid();
    EntityAddFlag(entity, ENTITY_FLAG_NONSPACIAL);
  }
}
