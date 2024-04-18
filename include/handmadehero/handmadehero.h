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

struct pairwise_collision_rule {
  u8 shouldCollide : 1;
  u32 storageIndexA;
  u32 storageIndexB;

  struct pairwise_collision_rule *next;
};

struct controlled_hero {
  u32 entityIndex;

  struct v3 ddPosition;
  f32 dZ;
  struct v2 dSword;
};

struct game_state {
  struct memory_arena worldArena;
  struct world *world;

  u32 followedEntityIndex;
  struct world_position cameraPosition;

  u32 storedEntityCount;
  struct stored_entity storedEntities[100000];

  struct controlled_hero controlledHeroes[HANDMADEHERO_CONTROLLER_COUNT];

  struct pairwise_collision_rule *collisionRules[256];
  struct pairwise_collision_rule *firstFreeCollisionRule;

  struct bitmap bitmapBackground;
  struct bitmap bitmapShadow;
  struct bitmap bitmapTree;
  struct bitmap bitmapSword;
  struct bitmap bitmapStairwell;

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

struct pairwise_collision_rule *
CollisionRuleGet(struct game_state *state, u32 storageIndexA);

void
CollisionRuleAdd(struct game_state *state, u32 storageIndexA, u32 storageIndexB, u8 shouldCollide);

#endif /* HANDMADEHERO_H */
