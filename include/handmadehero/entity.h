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

#endif /* HANDMADEHERO_ENTITY_H */
