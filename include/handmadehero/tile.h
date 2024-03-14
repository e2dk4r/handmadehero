#ifndef HANDMADEHERO_TILE_H
#define HANDMADEHERO_TILE_H

#include "types.h"

struct tile_chunk {
  u32 *tiles;
};

struct tile_map {
  f32 tileSideInMeters;
  u32 tileSideInPixels;
  f32 metersToPixels;

  u32 chunkShift;
  u32 chunkMask;
  u32 chunkDim;

  u32 tileChunkCountX;
  u32 tileChunkCountY;
  struct tile_chunk *tileChunks;
};

struct position_tile_map {
  /* packed. high bits for tile map x, low bits for tile x */
  u32 absTileX;
  /* packed. high bits for tile map y, low bits for tile y */
  u32 absTileY;

  f32 tileRelX;
  f32 tileRelY;
};

struct position_tile_chunk {
  u32 tileChunkX;
  u32 tileChunkY;

  u32 relTileX;
  u32 relTileY;
};

u32 TileGetValue(struct tile_map *tileMap, u32 absTileX, u32 absTileY);
void TileSetValue(struct tile_map *tileMap, u32 absTileX, u32 absTileY,
                  u32 value);
struct position_tile_map PositionCorrect(struct tile_map *tileMap,
                                         struct position_tile_map *pos);
#endif /* HANDMADEHERO_TILE_H */
