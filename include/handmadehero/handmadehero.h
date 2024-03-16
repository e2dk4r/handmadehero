#ifndef HANDMADEHERO_H

#include "memory_arena.h"
#include "platform.h"
#include "tile.h"

struct world {
  struct tile_map *tileMap;
};

struct game_state {
  struct memory_arena worldArena;
  struct world *world;

  struct position_tile_map playerPos;
  u32 *bitmap;
};

#define GAMEUPDATEANDRENDER(name)                                              \
  void name(struct game_memory *memory, struct game_input *input,              \
            struct game_backbuffer *backbuffer)
GAMEUPDATEANDRENDER(GameUpdateAndRender);
typedef GAMEUPDATEANDRENDER((*pfnGameUpdateAndRender));

#endif
