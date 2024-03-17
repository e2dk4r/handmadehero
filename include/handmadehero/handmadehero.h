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

struct bitmap_hero {
  struct bitmap head;
  struct bitmap torso;
  struct bitmap cape;
};

struct game_state {
  struct memory_arena worldArena;
  struct world *world;

  struct position_tile_map playerPos;
  struct bitmap bitmapBackground;

#define BITMAP_HERO_FRONT 3
#define BITMAP_HERO_BACK 1
#define BITMAP_HERO_LEFT 2
#define BITMAP_HERO_RIGHT 0
  u8 heroFacingDirection;
  struct bitmap_hero bitmapHero[4];
};

#define GAMEUPDATEANDRENDER(name)                                              \
  void name(struct game_memory *memory, struct game_input *input,              \
            struct game_backbuffer *backbuffer)
GAMEUPDATEANDRENDER(GameUpdateAndRender);
typedef GAMEUPDATEANDRENDER((*pfnGameUpdateAndRender));

#endif
