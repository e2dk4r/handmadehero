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

#define ENTITY_TYPE_INVALID 0
#define ENTITY_TYPE_HERO 1 << 0
#define ENTITY_TYPE_WALL 1 << 2

struct entity_low {
  u8 collides : 1;
  u8 type;
  f32 width;
  f32 height;
  struct position_tile_map position;
  u32 dAbsTileZ;
  u32 highIndex;
};

struct entity_high {
  struct v2 position;
  /* velocity, differencial of position */
  struct v2 dPosition;
  u32 absTileZ;
  u8 facingDirection;
  f32 z;
  f32 dZ;

  u32 lowIndex;
};

struct entity {
  struct entity_low *low;
  struct entity_high *high;
};

struct game_state {
  struct memory_arena worldArena;
  struct world *world;

  u32 followedEntityIndex;
  struct position_tile_map cameraPos;

#define HANDMADEHERO_ENTITY_HIGH_TOTAL 256
#define HANDMADEHERO_ENTITY_LOW_TOTAL 4096
  u32 entityLowCount;
  struct entity_low entityLows[HANDMADEHERO_ENTITY_HIGH_TOTAL];
  u32 entityHighCount;
  struct entity_high entityHighs[HANDMADEHERO_ENTITY_LOW_TOTAL];

  u32 playerIndexForController[HANDMADEHERO_CONTROLLER_COUNT];

  struct bitmap bitmapBackground;
  struct bitmap bitmapShadow;

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
