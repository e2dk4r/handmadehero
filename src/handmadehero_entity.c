#include <handmadehero/assert.h>
#include <handmadehero/entity.h>

inline u8
EntityIsFlagSet(struct entity *entity, enum entity_flag flag)
{
  return entity->flags & flag;
}

inline void
EntityAddFlag(struct entity *entity, enum entity_flag flag)
{
  entity->flags |= flag;
}

inline void
EntityClearFlag(struct entity *entity, enum entity_flag flag)
{
  entity->flags &= ~flag;
}
