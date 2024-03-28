#include <handmadehero/assert.h>
#include <handmadehero/math.h>
#include <handmadehero/tile.h>

#define TILE_CHUNK_SAFE_MARGIN 16

void TileMapInit(struct tile_map *tileMap, f32 tileSideInMeters) {
  tileMap->chunkShift = 4;
  tileMap->chunkDim = (u32)(1 << tileMap->chunkShift);
  tileMap->chunkMask = (u32)(1 << tileMap->chunkShift) - 1;

  tileMap->tileSideInMeters = tileSideInMeters;

  for (u32 tileChunkIndex = 0; tileChunkIndex < TILE_CHUNK_TOTAL;
       tileChunkIndex++) {
    tileMap->tileChunks[tileChunkIndex].tileChunkX = 0;
  }
}

static inline u32 TileChunkGetTileValue(struct tile_map *tileMap,
                                        struct tile_chunk *tileChunk, u32 x,
                                        u32 y) {
  assert(tileChunk);
  assert(x < tileMap->chunkDim);
  assert(y < tileMap->chunkDim);
  return tileChunk->tiles[y * tileMap->chunkDim + x];
}

static inline struct tile_chunk *
WorldGetTileChunk(struct tile_map *tileMap, u32 tileChunkX, u32 tileChunkY,
                  u32 tileChunkZ, struct memory_arena *arena) {
  assert(tileChunkX > TILE_CHUNK_SAFE_MARGIN);
  assert(tileChunkY > TILE_CHUNK_SAFE_MARGIN);
  assert(tileChunkZ > TILE_CHUNK_SAFE_MARGIN);
  assert(tileChunkX < U32_MAX - TILE_CHUNK_SAFE_MARGIN);
  assert(tileChunkY < U32_MAX - TILE_CHUNK_SAFE_MARGIN);
  assert(tileChunkZ < U32_MAX - TILE_CHUNK_SAFE_MARGIN);

  u32 hashValue = 19 * tileChunkX + 7 * tileChunkY + 3 * tileChunkZ;
  u32 hashSlot = hashValue & (TILE_CHUNK_TOTAL - 1);
  assert(hashSlot < TILE_CHUNK_TOTAL);

  struct tile_chunk *chunk = &tileMap->tileChunks[hashSlot];
  while (1) {
    /* match found */
    if (tileChunkX == chunk->tileChunkX && tileChunkY == chunk->tileChunkY &&
        tileChunkZ == chunk->tileChunkZ)
      break;

    /* if the chunk slot was filled and next is not filled
     * meaning we run out of end of the list
     */
    if (arena && chunk->tileChunkX != 0 && !chunk->next) {
      chunk->next = MemoryArenaPush(arena, sizeof(*chunk));
      chunk->tileChunkX = 0;
      chunk = chunk->next;
    }

    /* if we are on empty slot */
    if (arena && chunk->tileChunkX == 0) {
      chunk->tileChunkX = tileChunkX;
      chunk->tileChunkY = tileChunkY;
      chunk->tileChunkZ = tileChunkZ;

      u32 tileCount = tileMap->chunkDim * tileMap->chunkDim;
      chunk->tiles = MemoryArenaPush(arena, sizeof(u32) * tileCount);
      for (u32 tileIndex = 0; tileIndex < tileCount; tileIndex++) {
        chunk->tiles[tileIndex] = TILE_WALKABLE;
      }
      chunk->next = 0;
      break;
    }

    chunk = chunk->next;
    if (!chunk)
      break;
  }

  return chunk;
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
  assert(*tileRel <= 0.5f * tileMap->tileSideInMeters);
}

struct position_tile_map
PositionMapIntoTilesSpace(struct tile_map *tileMap,
                          struct position_tile_map *basePosition,
                          struct v2 offset) {
  struct position_tile_map result = *basePosition;
  v2_add_ref(&result.offset, offset);

  PositionCorrectCoord(tileMap, &result.absTileX, &result.offset.x);
  PositionCorrectCoord(tileMap, &result.absTileY, &result.offset.y);

  return result;
}

static inline struct position_tile_chunk
PositionTileChunkGet(struct tile_map *tileMap, u32 absTileX, u32 absTileY,
                     u32 absTileZ) {
  struct position_tile_chunk result;

  result.tileChunkX = absTileX >> tileMap->chunkShift;
  result.tileChunkY = absTileY >> tileMap->chunkShift;
  result.tileChunkZ = absTileZ;

  result.relTileX = absTileX & tileMap->chunkMask;
  result.relTileY = absTileY & tileMap->chunkMask;

  return result;
}

inline u32 TileGetValue(struct tile_map *tileMap, u32 absTileX, u32 absTileY,
                        u32 absTileZ) {
  struct position_tile_chunk chunkPos =
      PositionTileChunkGet(tileMap, absTileX, absTileY, absTileZ);
  struct tile_chunk *tileChunk =
      WorldGetTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY,
                        chunkPos.tileChunkZ, 0);
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
                  u32 absTileX, u32 absTileY, u32 absTileZ, u32 value) {
  struct position_tile_chunk chunkPos =
      PositionTileChunkGet(tileMap, absTileX, absTileY, absTileZ);
  struct tile_chunk *tileChunk =
      WorldGetTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY,
                        chunkPos.tileChunkZ, arena);
  assert(tileChunk);

  TileChunkSetTileValue(tileMap, tileChunk, chunkPos.relTileX,
                        chunkPos.relTileY, value);
}

struct position_difference PositionDifference(struct tile_map *tileMap,
                                              struct position_tile_map *a,
                                              struct position_tile_map *b) {
  struct position_difference result = {};

  struct v2 dTileXY = {
      .x = (f32)a->absTileX - (f32)b->absTileX,
      .y = (f32)a->absTileY - (f32)b->absTileY,
  };
  f32 dTileZ = (f32)a->absTileZ - (f32)b->absTileZ;

  result.dXY = v2_add(v2_mul(dTileXY, tileMap->tileSideInMeters),
                      v2_sub(a->offset, b->offset));
  result.dZ = tileMap->tileSideInMeters * dTileZ;

  return result;
}
