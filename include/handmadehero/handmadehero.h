#ifndef HANDMADEHERO_H
#define HANDMADEHERO_H

#include "memory_arena.h"
#include "platform.h"
#include "sim_region.h"
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

struct stored_entity {
  struct world_position position;
  struct entity sim;
};

struct controlled_hero {
  u32 entityIndex;

  struct v2 ddPosition;
  f32 dZ;
  struct v2 dSword;
};

struct game_state {
  struct memory_arena worldArena;
  struct world *world;

  u32 followedEntityIndex;
  struct world_position cameraPosition;

#define HANDMADEHERO_STORED_ENTITY_TOTAL 100000
  u32 storedEntityCount;
  struct stored_entity storedEntities[HANDMADEHERO_STORED_ENTITY_TOTAL];

  struct controlled_hero controlledHeroes[HANDMADEHERO_CONTROLLER_COUNT];

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

struct stored_entity *
StoredEntityGet(struct game_state *state, u32 index);

#endif /* HANDMADEHERO_H */
