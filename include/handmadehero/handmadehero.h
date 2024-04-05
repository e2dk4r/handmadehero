#ifndef HANDMADEHERO_H
#define HANDMADEHERO_H

#include "sim_region.h"
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

struct entity_low {
  struct world_position position;
  struct sim_entity sim;
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

#define HANDMADEHERO_STORED_ENTITY_TOTAL 100000
  u32 storedEntityCount;
  struct entity_low storedEntities[HANDMADEHERO_STORED_ENTITY_TOTAL];

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

void
GameUpdateAndRender(struct game_memory *memory, struct game_input *input, struct game_backbuffer *backbuffer);
typedef void (*pfnGameUpdateAndRender)(struct game_memory *memory, struct game_input *input,
                                       struct game_backbuffer *backbuffer);

void
EntityChangeLocation(struct memory_arena *arena, struct world *world, u32 entityLowIndex, struct entity_low *entityLow,
                     struct world_position *oldPosition, struct world_position *newPosition);

struct entity_low *
StoredEntityGet(struct game_state *state, u32 index);

#endif /* HANDMADEHERO_H */
