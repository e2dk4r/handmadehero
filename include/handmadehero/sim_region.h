#ifndef HANDMADEHERO_SIM_REGION_H
#define HANDMADEHERO_SIM_REGION_H

#include "math.h"
#include "types.h"
#include "world.h"

struct game_state;

enum entity_type {
  ENTITY_TYPE_INVALID = 0,
  ENTITY_TYPE_HERO = (1 << 0),
  ENTITY_TYPE_WALL = (1 << 1),
  ENTITY_TYPE_FAMILIAR = (1 << 2),
  ENTITY_TYPE_MONSTER = (1 << 3),
  ENTITY_TYPE_SWORD = (1 << 4),
  ENTITY_TYPE_STAIRWELL = (1 << 5),
  ENTITY_TYPE_SPACE = (1 << 6),
};

enum entity_flag {
  ENTITY_FLAG_COLLIDE = (1 << 0),
  ENTITY_FLAG_NONSPACIAL = (1 << 1),
  ENTITY_FLAG_ZSUPPORTED = (1 << 2),
  ENTITY_FLAG_TRAVERSABLE = (1 << 3)
};

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
  struct entity *ptr;
  u32 index;
};

struct entity_collision_volume {
  struct v3 offset;
  struct v3 dim;
};

struct entity_collision_volume_group {
  u32 volumeCount;
  struct entity_collision_volume *volumes;
  struct entity_collision_volume totalVolume;
};

struct entity {
  /* NOTE: These are only for sim region */
  u32 storageIndex;
  u8 updatable : 1;

  /**/

  enum entity_type type : 8;
  enum entity_flag flags : 4;

  struct v3 position;
  struct v3 dPosition;

  struct entity_collision_volume_group *collision;

  u32 hitPointMax;
  struct hit_point hitPoints[16];

  struct entity_reference sword;
  f32 distanceRemaining;

  u8 facingDirection;
  f32 tBob;

  f32 walkableHeight;
  struct v3 walkableDim;
};

struct sim_entity_hash {
  union {
    struct entity *ptr;
    u32 index;
  };
};

struct sim_region {
  struct world *world;
  f32 maxEntityRadius;
  f32 maxEntityVelocity;

  struct world_position origin;
  struct rect bounds;
  struct rect updatableBounds;

  u32 entityTotal;
  u32 entityCount;
  struct entity *entities;

  // NOTE: must be power of 2
  struct sim_entity_hash hashTable[4096];
};

struct sim_region *
BeginSimRegion(struct memory_arena *simArena, struct game_state *state, struct world *world,
               struct world_position regionCenter, struct rect regionBounds, f32 dt);

void
EndSimRegion(struct sim_region *region, struct game_state *state);

void
EntityMove(struct game_state *state, struct sim_region *simRegion, struct entity *entity, f32 dt,
           const struct move_spec *moveSpec, struct v3 ddPosition);

#endif /* HANDMADEHERO_SIM_REGION_H */
