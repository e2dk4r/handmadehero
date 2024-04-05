#ifndef HANDMADEHERO_SIM_REGION_H
#define HANDMADEHERO_SIM_REGION_H

#include "math.h"
#include "types.h"
#include "world.h"

struct game_state;

#define ENTITY_TYPE_INVALID 0
#define ENTITY_TYPE_HERO 1 << 0
#define ENTITY_TYPE_WALL 1 << 1
#define ENTITY_TYPE_FAMILIAR 1 << 2
#define ENTITY_TYPE_MONSTER 1 << 3
#define ENTITY_TYPE_SWORD 1 << 4

struct move_spec {
  u8 unitMaxAccel : 1;
  /* speed at m/s² */
  f32 speed;
  f32 drag;
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point {
  /* TODO: bake this down to one variable */
  u8 flags;
  u8 filledAmount;
};

struct entity_reference {
  union {
    struct sim_entity *ptr;
    u32 index;
  };
};

struct sim_entity {
  u32 storageIndex;

  u8 type;

  struct v2 position;
  u32 chunkZ;

  f32 z;
  f32 dZ;

  u8 collides : 1;
  f32 width;
  f32 height;
  u32 dAbsTileZ;
  u32 highIndex;

  u32 hitPointMax;
  struct hit_point hitPoints[16];

  struct entity_reference sword;
  f32 distanceRemaining;

  u8 facingDirection;
  f32 tBob;
};

struct sim_entity_hash {
  struct sim_entity *ptr;
  u32 index;
};

struct sim_region {
  struct world *world;

  struct world_position origin;
  struct rectangle2 bounds;

  u32 entityTotal;
  u32 entityCount;
  struct sim_entity *entities;

  // NOTE: must be power of 2
  struct sim_entity_hash hashTable[4096];
};

struct sim_region *
BeginSimRegion(struct memory_arena *simArena, struct game_state *state, struct world *world,
               struct world_position regionCenter, struct rectangle2 regionBounds);
void
EndSimRegion(struct sim_region *region, struct game_state *state);

#endif /* HANDMADEHERO_SIM_REGION_H */
