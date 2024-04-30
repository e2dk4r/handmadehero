#include <handmadehero/render_group.h>

struct render_group *
RenderGroup(struct memory_arena *arena, u64 pushBufferTotal, f32 metersToPixels)
{
  struct render_group *group = MemoryArenaPush(arena, sizeof(*group));

  group->defaultBasis = MemoryArenaPush(arena, sizeof(*group->defaultBasis));
  group->defaultBasis->position = v3(0.0f, 0.0f, 0.0f);

  group->metersToPixels = metersToPixels;

  group->pushBufferSize = 0;
  group->pushBufferTotal = pushBufferTotal;
  group->pushBufferBase = MemoryArenaPush(arena, group->pushBufferTotal);

  return group;
}

internal inline void *
PushRenderEntity(struct render_group *group, u32 size)
{
  void *result = 0;

  assert(group->pushBufferSize + size <= group->pushBufferTotal);

  result = group->pushBufferBase + group->pushBufferSize;
  group->pushBufferSize += size;

  return result;
}

internal inline void
PushEntityWithZ(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
                struct v2 dim, struct v4 color, f32 z)
{
  struct render_group_entry *entity = PushRenderEntity(group, sizeof(*entity));
  entity->basis = group->defaultBasis;
  entity->bitmap = bitmap;
  entity->offset = v2_mul(v2_hadamard(offset, v2(1.0f, -1.0f)), group->metersToPixels);
  entity->offsetZ = offsetZ;
  entity->align = align;
  entity->cZ = z;
  entity->dim = dim;
  entity->color = color;
}

internal inline void
PushEntity(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
           struct v2 dim, struct v4 color)
{
  PushEntityWithZ(group, bitmap, offset, offsetZ, align, dim, color, 1.0f);
}

inline void
PushBitmap(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align)
{
  PushEntity(group, bitmap, offset, offsetZ, align, v2(0.0f, 0.0f), v4(1.0f, 1.0f, 1.0f, 1.0f));
}

inline void
PushBitmapWithAlpha(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
                    f32 alpha)
{
  PushEntity(group, bitmap, offset, offsetZ, align, v2(0.0f, 0.0f), v4(1.0f, 1.0f, 1.0f, alpha));
}

inline void
PushBitmapWithAlphaAndZ(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ,
                        struct v2 align, f32 alpha, f32 z)
{
  PushEntityWithZ(group, bitmap, offset, offsetZ, align, v2(0.0f, 0.0f), v4(1.0f, 1.0f, 1.0f, alpha), z);
}

inline void
PushRect(struct render_group *group, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color)
{
  PushEntity(group, 0, offset, offsetZ, v2(0.0f, 0.0f), dim, color);
}

inline void
PushRectOutline(struct render_group *group, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color)
{
  f32 thickness = 0.1f;

  // NOTE(e2dk4r): top and bottom
  PushEntity(group, 0, v2_sub(offset, v2(0.0f, 0.5f * dim.y)), offsetZ, v2(0.0f, 0.0f), v2(dim.x, thickness), color);
  PushEntity(group, 0, v2_add(offset, v2(0.0f, 0.5f * dim.y)), offsetZ, v2(0.0f, 0.0f), v2(dim.x, thickness), color);

  // NOTE(e2dk4r): left right
  PushEntity(group, 0, v2_sub(offset, v2(0.5f * dim.x, 0.0f)), offsetZ, v2(0.0f, 0.0f), v2(thickness, dim.y), color);
  PushEntity(group, 0, v2_add(offset, v2(0.5f * dim.x, 0.0f)), offsetZ, v2(0.0f, 0.0f), v2(thickness, dim.y), color);
}
