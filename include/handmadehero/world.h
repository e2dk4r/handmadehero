#ifndef HANDMADEHERO_WORLD_H
#define HANDMADEHERO_WORLD_H

#include "math.h"
#include "memory_arena.h"
#include "types.h"

#define TILE_INVALID 0
#define TILE_WALKABLE (1 << 0)
#define TILE_BLOCKED (1 << 1)
#define TILE_LADDER_UP (1 << 2)
#define TILE_LADDER_DOWN (1 << 3)

struct world_entity_block {
  u32 entityCount;
  u32 lowEntities[16];
  struct world_entity_block *next;
};

struct world_chunk {
  u32 chunkX;
  u32 chunkY;
  u32 chunkZ;

  struct world_entity_block firstBlock;
  struct world_chunk *next;
};

#define WORLD_CHUNK_TOTAL 4096
struct world {
  f32 tileSideInMeters;

  u32 chunkShift;
  u32 chunkMask;
  u32 chunkDim;
  struct world_chunk worldChunkHash[WORLD_CHUNK_TOTAL];
};

struct world_position {
  /* packed. high bits for tile map x, low bits for tile x */
  u32 absTileX;
  /* packed. high bits for tile map y, low bits for tile y */
  u32 absTileY;
  /* packed. high bits for tile map y, low bits for tile y */
  u32 absTileZ;

  /* offset from tile center */
  struct v2 offset;
};

struct world_difference {
  struct v2 dXY;
  f32 dZ;
};

void WorldInit(struct world *world, f32 tileSideInMeters);
static inline struct world_position
WorldPositionCentered(u32 absTileX, u32 absTileY, u32 absTileZ) {
  return (struct world_position){absTileX, absTileY, absTileZ, 0, 0};
}

struct world_position
WorldPositionCalculate(struct world *world, struct world_position *basePosition,
                       struct v2 offset);

struct world_difference WorldPositionSub(struct world *world,
                                         struct world_position *a,
                                         struct world_position *b);

static inline u8 WorldPositionSame(struct world_position *left,
                                   struct world_position *right) {
  return
      /* x */
      left->absTileX == right->absTileX
      /* y */
      && left->absTileY == right->absTileY
      /* z */
      && left->absTileZ == right->absTileZ;
}

#endif /* HANDMADEHERO_WORLD_H */
