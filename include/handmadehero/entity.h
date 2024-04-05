#ifndef HANDMADEHERO_ENTITY_H
#define HANDMADEHERO_ENTITY_H

#include "types.h"
#include "sim_region.h"

void
EntityUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt);

void
FamiliarUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt);

void
MonsterUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt);

void
SwordUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt);

#endif /* HANDMADEHERO_ENTITY_H */
