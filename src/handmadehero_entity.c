#include <handmadehero/assert.h>
#include <handmadehero/entity.h>

comptime struct move_spec FamiliarMoveSpec = {
    .unitMaxAccel = 1,
    .speed = 30.0f,
    .drag = 8.0f,
};

comptime struct move_spec SwordMoveSpec = {
    .unitMaxAccel = 0,
    .speed = 0.0f,
    .drag = 0.0f,
};

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

inline void
FamiliarUpdate(struct sim_region *simRegion, struct entity *familiar, f32 dt)
{
  struct entity *closestHero = 0;
  /* 10m maximum search radius */
  f32 closestHeroDistanceSq = square(10.0f);

  for (u32 entityIndex = 0; entityIndex < simRegion->entityCount; entityIndex++) {
    struct entity *testEntity = simRegion->entities + entityIndex;

    if (testEntity->type == ENTITY_TYPE_INVALID || !(testEntity->type & ENTITY_TYPE_HERO))
      continue;

    struct entity *heroEntity = testEntity;

    f32 testDistanceSq = v2_length_square(v2_sub(familiar->position, heroEntity->position));
    if (testDistanceSq < closestHeroDistanceSq) {
      closestHero = heroEntity;
      closestHeroDistanceSq = testDistanceSq;
    }
  }

  struct v2 ddPosition = {};
  if (closestHero && closestHeroDistanceSq > square(3.0f)) {
    /* there is hero nearby, follow him */

    f32 oneOverLength = 1.0f / SquareRoot(closestHeroDistanceSq);
    ddPosition = v2_mul(v2_sub(closestHero->position, familiar->position), oneOverLength);
  }

  EntityMove(simRegion, familiar, dt, &FamiliarMoveSpec, ddPosition);
}

inline void
MonsterUpdate(struct sim_region *simRegion, struct entity *entity, f32 dt)
{
}

inline void
SwordUpdate(struct sim_region *simRegion, struct entity *sword, f32 dt)
{
  struct v2 oldPosition = sword->position;
  EntityMove(simRegion, sword, dt, &SwordMoveSpec, v2(0, 0));
  f32 distanceTraveled = v2_length(v2_sub(sword->position, oldPosition));

  sword->distanceRemaining -= distanceTraveled;
  if (sword->distanceRemaining < 0.0f) {
    assert(0 && "need to make entities disappear");
  }
}
