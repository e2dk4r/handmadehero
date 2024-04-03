#ifndef HANDMADEHERO_H

#include "memory_arena.h"
#include "platform.h"
#include "world.h"

struct bitmap {
  u32 width;
  u32 height;
  u32 *pixels;
};

struct bitmap_hero {
  struct v2 align;

  struct bitmap head;
  struct bitmap torso;
  struct bitmap cape;
};

#define ENTITY_TYPE_INVALID 0
#define ENTITY_TYPE_HERO 1 << 0
#define ENTITY_TYPE_WALL 1 << 1
#define ENTITY_TYPE_FAMILIAR 1 << 2
#define ENTITY_TYPE_MONSTER 1 << 3
#define ENTITY_TYPE_SWORD 1 << 4

#define HIT_POINT_SUB_COUNT 4
struct hit_point {
  /* TODO: bake this down to one variable */
  u8 flags;
  u8 filledAmount;
};

struct entity_low {
  u8 collides : 1;
  u8 type;
  f32 width;
  f32 height;
  struct world_position position;
  u32 dAbsTileZ;
  u32 highIndex;

  u32 hitPointMax;
  struct hit_point hitPoints[16];

  u32 swordLowIndex;
  f32 distanceRemaining;
};

struct entity_high {
  struct v2 position;
  /* velocity, differencial of position */
  struct v2 dPosition;
  u32 chunkZ;
  u8 facingDirection;
  f32 z;
  f32 dZ;

  f32 tBob;

  u32 lowIndex;
};

struct entity {
  u32 lowIndex;
  struct entity_low *low;
  struct entity_high *high;
};

struct game_state {
  struct memory_arena worldArena;
  struct world *world;

  u32 followedEntityIndex;
  struct world_position cameraPos;

#define HANDMADEHERO_ENTITY_LOW_TOTAL 100000
  u32 entityLowCount;
  struct entity_low entityLows[HANDMADEHERO_ENTITY_LOW_TOTAL];

#define HANDMADEHERO_ENTITY_HIGH_TOTAL 256
  u32 entityHighCount;
  struct entity_high entityHighs[HANDMADEHERO_ENTITY_HIGH_TOTAL];

  u32 playerIndexForController[HANDMADEHERO_CONTROLLER_COUNT];

  struct bitmap bitmapBackground;
  struct bitmap bitmapShadow;
  struct bitmap bitmapTree;
  struct bitmap bitmapSword;

#define BITMAP_HERO_FRONT 3
#define BITMAP_HERO_BACK 1
#define BITMAP_HERO_LEFT 2
#define BITMAP_HERO_RIGHT 0
  struct bitmap_hero bitmapHero[4];
};

void GameUpdateAndRender(struct game_memory *memory, struct game_input *input,
                         struct game_backbuffer *backbuffer);
typedef void (*pfnGameUpdateAndRender)(struct game_memory *memory,
                                       struct game_input *input,
                                       struct game_backbuffer *backbuffer);

#endif
