#ifndef HANDMADEHERO_ENTITY_H
#define HANDMADEHERO_ENTITY_H

#include "sim_region.h"
#include "types.h"

u8
EntityIsFlagSet(struct entity *entity, enum entity_flag flag);

void
EntityAddFlag(struct entity *entity, enum entity_flag flag);

void
EntityClearFlag(struct entity *entity, enum entity_flag flag);

#endif /* HANDMADEHERO_ENTITY_H */
