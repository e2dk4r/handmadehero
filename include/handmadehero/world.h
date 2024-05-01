#ifndef HANDMADEHERO_WORLD_H
#define HANDMADEHERO_WORLD_H

#include "math.h"
#include "memory_arena.h"
#include "types.h"

struct stored_entity;

struct world_entity_block {
  u32 entityCount;
  u32 entityLowIndexes[16];
  struct world_entity_block *next;
};

struct world_chunk {
  u32 chunkX;
  u32 chunkY;
  u32 chunkZ;

  struct world_entity_block firstBlock;
  struct world_chunk *next;
};

struct world {
  struct v3 chunkDimInMeters;

  struct world_entity_block *firstFreeBlock;
  struct world_chunk chunkHash[4096];
};

struct world_position {
  u32 chunkX;
  u32 chunkY;
  u32 chunkZ;

  /* offset from chunk center */
  struct v3 offset;
};

internal inline struct world_position
WorldPositionInvalid()
{
  struct world_position result = {};
  return result;
}

internal inline u8
WorldPositionIsValid(struct world_position *position)
{
  return position->chunkX != 0;
}

void
WorldInit(struct world *world, struct v3 chunkDimInMeters);

struct world_chunk *
WorldChunkGet(struct world *world, u32 chunkX, u32 chunkY, u32 chunkZ);

internal inline struct world_position
WorldPositionCentered(u32 absTileX, u32 absTileY, u32 absTileZ)
{
  return (struct world_position){absTileX, absTileY, absTileZ, 0, 0, 0};
}

struct world_position
WorldPositionCalculate(struct world *world, struct world_position *basePosition, struct v3 offset);

struct v3
WorldPositionSub(struct world *world, struct world_position *a, struct world_position *b);

void
EntityChangeLocation(struct memory_arena *arena, struct world *world, u32 entityLowIndex, struct stored_entity *stored,
                     struct world_position *oldPosition, struct world_position *newPosition);

u8
IsWorldPositionOffsetCalculated(struct world *world, struct v3 *offset);

u8
IsWorldPositionSame(struct world *world, struct world_position *left, struct world_position *right);

#endif /* HANDMADEHERO_WORLD_H */
