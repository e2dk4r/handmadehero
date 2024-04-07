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

inline void
EntityUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt)
{
  /* apply gravity */
  f32 ddZ = -9.8f;
  entity->z +=
      /* 1/2 a t² */
      0.5f * ddZ * square(dt)
      /* + v t */
      + entity->dZ * dt;
  entity->dZ +=
      /* a t */
      ddZ * dt;
  if (entity->z < 0)
    entity->z = 0;
}
