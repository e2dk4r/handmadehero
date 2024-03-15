#include <handmadehero/assert.h>
#include <handmadehero/math.h>
#include <handmadehero/tile.h>

static inline u32 TileChunkGetTileValue(struct tile_map *tileMap,
                                        struct tile_chunk *tileChunk, u32 x,
                                        u32 y) {
  assert(tileChunk);
  assert(x < tileMap->chunkDim);
  assert(y < tileMap->chunkDim);
  return tileChunk->tiles[y * tileMap->chunkDim + x];
}

static inline struct tile_chunk *
WorldGetTileChunk(struct tile_map *tileMap, u32 tileChunkX, u32 tileChunkY) {
  if (tileChunkX > tileMap->tileChunkCountX)
    return 0;

  if (tileChunkY > tileMap->tileChunkCountY)
    return 0;

  return &tileMap
              ->tileChunks[tileChunkY * tileMap->tileChunkCountX + tileChunkX];
}

static inline void PositionCorrectCoord(struct tile_map *tileMap, u32 *tile,
                                        f32 *tileRel) {
  /* NOTE: tileMap is assumed to be toroidal topology, if you step off one end
   * you come back on other.
   */

  i32 offset = roundf32toi32(*tileRel / tileMap->tileSideInMeters);
  *tile += (u32)offset;
  *tileRel -= (f32)offset * tileMap->tileSideInMeters;

  assert(*tileRel >= -0.5f * tileMap->tileSideInMeters);
  assert(*tileRel < 0.5f * tileMap->tileSideInMeters);
}

struct position_tile_map PositionCorrect(struct tile_map *tileMap,
                                         struct position_tile_map *pos) {
  struct position_tile_map result = *pos;

  PositionCorrectCoord(tileMap, &result.absTileX, &result.tileRelX);
  PositionCorrectCoord(tileMap, &result.absTileY, &result.tileRelY);

  return result;
}

static inline struct position_tile_chunk
PositionTileChunkGet(struct tile_map *tileMap, u32 absTileX, u32 absTileY) {
  struct position_tile_chunk result;

  result.tileChunkX = absTileX >> tileMap->chunkShift;
  result.tileChunkY = absTileY >> tileMap->chunkShift;

  result.relTileX = absTileX & tileMap->chunkMask;
  result.relTileY = absTileY & tileMap->chunkMask;

  return result;
}

inline u32 TileGetValue(struct tile_map *tileMap, u32 absTileX, u32 absTileY) {
  struct position_tile_chunk chunkPos =
      PositionTileChunkGet(tileMap, absTileX, absTileY);
  struct tile_chunk *tileChunk =
      WorldGetTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY);
  assert(tileChunk);

  if (!tileChunk->tiles)
    return TILE_INVALID;

  u32 value = TileChunkGetTileValue(tileMap, tileChunk, chunkPos.relTileX,
                                    chunkPos.relTileY);

  return value;
}

static inline void TileChunkSetTileValue(struct tile_map *tileMap,
                                         struct tile_chunk *tileChunk, u32 x,
                                         u32 y, u32 value) {
  assert(tileChunk);
  assert(tileChunk->tiles);
  assert(x < tileMap->chunkDim);
  assert(y < tileMap->chunkDim);
  tileChunk->tiles[y * tileMap->chunkDim + x] = value;
}

void TileSetValue(struct memory_arena *arena, struct tile_map *tileMap,
                  u32 absTileX, u32 absTileY, u32 value) {
  struct position_tile_chunk chunkPos =
      PositionTileChunkGet(tileMap, absTileX, absTileY);
  struct tile_chunk *tileChunk =
      WorldGetTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY);
  assert(tileChunk);

  /* allocate tiles for new location. sparse tile storage */
  if (!tileChunk->tiles) {
    u32 tileCount = tileMap->chunkDim * tileMap->chunkDim;
    tileChunk->tiles = MemoryArenaPush(arena, sizeof(u32) * tileCount);
    for (u32 tileIndex = 0; tileIndex < tileCount; tileIndex++) {
      tileChunk->tiles[tileIndex] = TILE_WALKABLE;
    }
  }

  TileChunkSetTileValue(tileMap, tileChunk, chunkPos.relTileX,
                        chunkPos.relTileY, value);
}
