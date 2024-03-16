#ifndef HANDMADEHERO_H

#include "memory_arena.h"
#include "platform.h"
#include "tile.h"

struct world {
  struct tile_map *tileMap;
};

struct bitmap {
  u32 width;
  u32 height;
  u32 *pixels;
};

struct game_state {
  struct memory_arena worldArena;
  struct world *world;

  struct position_tile_map playerPos;
  struct bitmap bitmapBackground;
};

#define GAMEUPDATEANDRENDER(name)                                              \
  void name(struct game_memory *memory, struct game_input *input,              \
            struct game_backbuffer *backbuffer)
GAMEUPDATEANDRENDER(GameUpdateAndRender);
typedef GAMEUPDATEANDRENDER((*pfnGameUpdateAndRender));

#endif
