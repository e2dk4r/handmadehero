#include <handmadehero/assert.h>
#include <handmadehero/entity.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/math.h>
#include <handmadehero/world.h>

#define WORLD_CHUNK_SAFE_MARGIN 16
#define WORLD_CHUNK_UNINITIALIZED 0

struct world_position
WorldPositionInvalid(void)
{
  struct world_position result = {};
  return result;
}

inline u8
WorldPositionIsValid(struct world_position *position)
{
  if (!position)
    return 0;

  return position->chunkX != WORLD_CHUNK_UNINITIALIZED;
}

void
WorldInit(struct world *world, struct v3 chunkDimInMeters)
{
  world->chunkDimInMeters = chunkDimInMeters;
  world->firstFreeBlock = 0;

  for (u32 chunkIndex = 0; chunkIndex < ARRAY_COUNT(world->chunkHash); chunkIndex++) {
    struct world_chunk *chunk = world->chunkHash + chunkIndex;
    chunk->chunkX = WORLD_CHUNK_UNINITIALIZED;
    chunk->firstBlock.entityCount = 0;
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

  // TODO(e2dk4r): Better hash function!
  u32 hashValue = 19 * chunkX + 7 * chunkY + 3 * chunkZ;
  u32 hashSlot = hashValue & (ARRAY_COUNT(world->chunkHash) - 1);
  assert(hashSlot < ARRAY_COUNT(world->chunkHash));

  struct world_chunk *chunk = world->chunkHash + hashSlot;
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
      chunk->chunkX = WORLD_CHUNK_UNINITIALIZED;
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
IsWorldPositionCalculated(f32 chunkDim, f32 chunkRel)
{
  const f32 epsilon = 0.005f;
  f32 max = 0.5f * chunkDim + epsilon;
  f32 min = -max;
  return (chunkRel >= min) && (chunkRel <= max);
}

inline u8
IsWorldPositionOffsetCalculated(struct world *world, struct v3 *offset)
{
  return IsWorldPositionCalculated(world->chunkDimInMeters.x, offset->x) &&
         IsWorldPositionCalculated(world->chunkDimInMeters.y, offset->y) &&
         IsWorldPositionCalculated(world->chunkDimInMeters.z, offset->z);
}

internal inline void
WorldPositionCalculateAxis(f32 chunkDim, u32 *chunk, f32 *chunkRel)
{
  /* NOTE: world is assumed to be toroidal topology, if you step off one end
   * you come back on other.
   */

  s32 offset = roundf32tos32(*chunkRel / chunkDim);
  *chunk += (u32)offset;
  *chunkRel -= (f32)offset * chunkDim;

  assert(IsWorldPositionCalculated(chunkDim, *chunkRel));
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

inline u8
IsChunkPositionSame(struct world *world, struct world_position *left, struct world_position *right)
{
  assert(IsWorldPositionOffsetCalculated(world, &left->offset));
  assert(IsWorldPositionOffsetCalculated(world, &right->offset));
  return
      /* x */
      left->chunkX == right->chunkX
      /* y */
      && left->chunkY == right->chunkY
      /* z */
      && left->chunkZ == right->chunkZ;
}

internal inline void
EntityChangeLocationRaw(struct memory_arena *arena, struct world *world, u32 storageIndex,
                        struct world_position *oldPosition, struct world_position *newPosition)
{
  assert(!oldPosition || WorldPositionIsValid(oldPosition));
  assert(!newPosition || WorldPositionIsValid(newPosition));

  if (oldPosition && newPosition && IsChunkPositionSame(world, oldPosition, newPosition))
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

        if (block->entityStorageIndexes[blockEntityIndex] != storageIndex)
          continue;

        u32 firstBlockEntityLastIndex = firstBlock->entityCount - 1;
        block->entityStorageIndexes[blockEntityIndex] = firstBlock->entityStorageIndexes[firstBlockEntityLastIndex];
        firstBlock->entityCount--;

        if (firstBlock->entityCount == 0) {
          if (firstBlock->next) {
            struct world_entity_block *nextBlock = firstBlock->next;
            *firstBlock = *firstBlock->next;

            nextBlock->next = world->firstFreeBlock;
            world->firstFreeBlock = nextBlock;
          }
        }

        found = 1;
      }
    }
  }

  if (newPosition) {
    // insert into its new entity block
    struct world_chunk *chunk =
        WorldChunkGetOrInsert(world, newPosition->chunkX, newPosition->chunkY, newPosition->chunkZ, arena);
    struct world_entity_block *block = &chunk->firstBlock;
    if (block->entityCount == ARRAY_COUNT(block->entityStorageIndexes)) {
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

    assert(block->entityCount < ARRAY_COUNT(block->entityStorageIndexes));
    block->entityStorageIndexes[block->entityCount] = storageIndex;
    block->entityCount++;
  }
}

inline void
EntityChangeLocation(struct memory_arena *arena, struct world *world, struct stored_entity *stored,
                     struct world_position *newPosition)
{
  struct entity *entity = &stored->sim;

  struct world_position *oldPosition = 0;
  if (WorldPositionIsValid(&stored->position))
    oldPosition = &stored->position;

  struct world_position *newPositionToBeApplied = 0;
  if (WorldPositionIsValid(newPosition))
    newPositionToBeApplied = newPosition;

  EntityChangeLocationRaw(arena, world, entity->storageIndex, oldPosition, newPositionToBeApplied);

  if (newPosition) {
    stored->position = *newPosition;
    EntityClearFlag(entity, ENTITY_FLAG_NONSPACIAL);
  } else {
    stored->position = WorldPositionInvalid();
    EntityAddFlag(entity, ENTITY_FLAG_NONSPACIAL);
  }
}
