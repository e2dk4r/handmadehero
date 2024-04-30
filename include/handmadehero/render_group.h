#ifndef HANDMADEHERO_RENDER_GROUP
#define HANDMADEHERO_RENDER_GROUP

#include "math.h"
#include "memory_arena.h"
#include "types.h"

struct game_state;
struct bitmap;

struct render_basis {
  struct v3 position;
};

struct render_entity {
  struct render_basis *basis;
  struct bitmap *bitmap;
  struct v2 offset;
  struct v2 align;
  f32 offsetZ;
  f32 cZ;

  struct v4 color;
  struct v2 dim;
};

struct render_group {
  struct render_basis *defaultBasis;
  f32 metersToPixels;

  u64 pushBufferTotal;
  u64 pushBufferSize;
  void *pushBufferBase;
};

struct render_group *
RenderGroup(struct memory_arena *arena, u64 pushBufferTotal, f32 metersToPixels);

void
PushBitmap(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align);

void
PushBitmapWithAlpha(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
                    f32 alpha);

void
PushBitmapWithZ(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
                f32 z);

void
PushBitmapWithAlphaAndZ(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ,
                        struct v2 align, f32 alpha, f32 z);

void
PushRect(struct render_group *group, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color);

void
PushRectOutline(struct render_group *group, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color);

#endif /* HANDMADEHERO_RENDER_GROUP */
