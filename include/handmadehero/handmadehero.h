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
  i32 alignX;
  i32 alignY;

  struct bitmap head;
  struct bitmap torso;
  struct bitmap cape;
};

struct entity {
  u8 exists : 1;
  u8 facingDirection;
  f32 width;
  f32 height;
  struct position_tile_map position;
  struct v2 dPosition;
};

struct game_state {
  struct memory_arena worldArena;
  struct world *world;

  u32 followedEntityIndex;
  struct position_tile_map cameraPos;

#define HANDMADEHERO_ENTITY_TOTAL 256
  struct entity entities[HANDMADEHERO_ENTITY_TOTAL];
  u32 entityCount;
  u32 playerIndexForController[HANDMADEHERO_CONTROLLER_COUNT];

  struct bitmap bitmapBackground;

#define BITMAP_HERO_FRONT 3
#define BITMAP_HERO_BACK 1
#define BITMAP_HERO_LEFT 2
#define BITMAP_HERO_RIGHT 0
  struct bitmap_hero bitmapHero[4];
};

#define GAMEUPDATEANDRENDER(name)                                              \
  void name(struct game_memory *memory, struct game_input *input,              \
            struct game_backbuffer *backbuffer)
GAMEUPDATEANDRENDER(GameUpdateAndRender);
typedef GAMEUPDATEANDRENDER((*pfnGameUpdateAndRender));

#endif
