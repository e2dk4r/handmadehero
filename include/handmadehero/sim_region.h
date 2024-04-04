#ifndef HANDMADEHERO_SIM_REGION_H
#define HANDMADEHERO_SIM_REGION_H

#include "handmadehero.h"
#include "math.h"
#include "types.h"
#include "world.h"

struct sim_entity {
  u32 storageIndex;

  struct v2 position;
  u32 chunkZ;

  f32 z;
  f32 dZ;
};

struct sim_region {
  struct world *world;

  struct world_position origin;
  struct rectangle2 bounds;

  u32 entityTotal;
  u32 entityCount;
  struct sim_entity *entities;
};

struct sim_region *
BeginSimRegion(struct memory_arena *simArena, struct game_state *state, struct world *world,
               struct world_position regionCenter, struct rectangle2 regionBounds);
void
EndSimRegion(struct sim_region *region, struct game_state *state);

#endif /* HANDMADEHERO_SIM_REGION_H */
