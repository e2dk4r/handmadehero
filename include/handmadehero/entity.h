#ifndef HANDMADEHERO_ENTITY_H
#define HANDMADEHERO_ENTITY_H

#include "sim_region.h"
#include "types.h"

u8
EntityIsFlagSet(struct entity *entity, u8 flag);

void
EntityAddFlag(struct entity *entity, u8 flag);

void
EntityClearFlag(struct entity *entity, u8 flag);

void
EntityUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt);

void
FamiliarUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt);

void
MonsterUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt);

void
SwordUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt);

#endif /* HANDMADEHERO_ENTITY_H */
