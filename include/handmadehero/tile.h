#ifndef HANDMADEHERO_TILE_H
#define HANDMADEHERO_TILE_H

#include "memory_arena.h"
#include "types.h"
#include "math.h"

#define TILE_INVALID 0
#define TILE_WALKABLE (1 << 0)
#define TILE_BLOCKED (1 << 1)
#define TILE_LADDER_UP (1 << 2)
#define TILE_LADDER_DOWN (1 << 3)

struct tile_chunk {
  u32 *tiles;
};

struct tile_map {
  f32 tileSideInMeters;

  u32 chunkShift;
  u32 chunkMask;
  u32 chunkDim;

  u32 tileChunkCountX;
  u32 tileChunkCountY;
  u32 tileChunkCountZ;
  struct tile_chunk *tileChunks;
};

struct position_tile_map {
  /* packed. high bits for tile map x, low bits for tile x */
  u32 absTileX;
  /* packed. high bits for tile map y, low bits for tile y */
  u32 absTileY;
  /* packed. high bits for tile map y, low bits for tile y */
  u32 absTileZ;

  /* offset from tile center */
  struct v2 offset;
};

struct position_tile_chunk {
  u32 tileChunkX;
  u32 tileChunkY;
  u32 tileChunkZ;

  u32 relTileX;
  u32 relTileY;
};

struct position_difference {
  struct v2 dXY;
  f32 dZ;
};

struct position_difference PositionDifference(struct tile_map *tileMap,
                                              struct position_tile_map *a,
                                              struct position_tile_map *b);

static inline struct position_tile_map
PositionTileMapCentered(u32 absTileX, u32 absTileY, u32 absTileZ) {
  return (struct position_tile_map){absTileX, absTileY, absTileZ, 0, 0};
}

u32 TileGetValue(struct tile_map *tileMap, u32 absTileX, u32 absTileY,
                 u32 absTileZ);

static inline u32 TileGetValue2(struct tile_map *tileMap,
                                struct position_tile_map *pos) {
  return TileGetValue(tileMap, pos->absTileX, pos->absTileY, pos->absTileZ);
}

void TileSetValue(struct memory_arena *arena, struct tile_map *tileMap,
                  u32 absTileX, u32 absTileY, u32 absTileZ, u32 value);
struct position_tile_map PositionCorrect(struct tile_map *tileMap,
                                         struct position_tile_map *pos);

static inline u8 PositionTileMapSameTile(struct position_tile_map *left,
                                         struct position_tile_map *right) {
  return
      /* x */
      left->absTileX == right->absTileX
      /* y */
      && left->absTileY == right->absTileY
      /* z */
      && left->absTileZ == right->absTileZ;
}

static inline u8 TileIsEmpty(u32 value) {
  return value & (TILE_WALKABLE | TILE_LADDER_UP | TILE_LADDER_DOWN);
}

static inline u8 TileMapIsPointEmpty(struct tile_map *tileMap,
                                     struct position_tile_map *testPos) {
  u32 value = TileGetValue(tileMap, testPos->absTileX, testPos->absTileY,
                           testPos->absTileZ);
  return TileIsEmpty(value);
}

#endif /* HANDMADEHERO_TILE_H */
