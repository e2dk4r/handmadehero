#include <handmadehero/assert.h>
#include <handmadehero/entity.h>

inline u8
EntityIsFlagSet(struct entity *entity, u8 flag)
{
  return entity->flags & flag;
}

inline void
EntityAddFlag(struct entity *entity, u8 flag)
{
  entity->flags |= flag;
}

inline void
EntityClearFlag(struct entity *entity, u8 flag)
{
  entity->flags &= ~flag;
}
