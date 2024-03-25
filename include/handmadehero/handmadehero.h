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

#define ENTITY_RESIDENCE_NONEXISTENT 0
#define ENTITY_RESIDENCE_DORMANT 1 << 0
#define ENTITY_RESIDENCE_LOW 1 << 1
#define ENTITY_RESIDENCE_HIGH 1 << 2

struct entity_dormant {
  f32 width;
  f32 height;
  struct position_tile_map position;
};

struct entity_low {};

struct entity_high {
  struct v2 position;
  /* velocity, differencial of position */
  struct v2 dPosition;
  u8 facingDirection;
};

struct entity {
  u8 residence;
  struct entity_dormant *dormant;
  struct entity_low *low;
  struct entity_high *high;
};

struct game_state {
  struct memory_arena worldArena;
  struct world *world;

  u32 followedEntityIndex;
  struct position_tile_map cameraPos;

#define HANDMADEHERO_ENTITY_TOTAL 256
  struct entity entities[HANDMADEHERO_ENTITY_TOTAL];
  u8 entityResidences[HANDMADEHERO_ENTITY_TOTAL];
  struct entity_dormant entityDormants[HANDMADEHERO_ENTITY_TOTAL];
  struct entity_low entityLows[HANDMADEHERO_ENTITY_TOTAL];
  struct entity_high entityHighs[HANDMADEHERO_ENTITY_TOTAL];
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
