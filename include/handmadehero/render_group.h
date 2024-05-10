#ifndef HANDMADEHERO_RENDER_GROUP
#define HANDMADEHERO_RENDER_GROUP

/* NOTE(e2dk4r):
 *
 * 1. Everywhere outside of renderer, Y **always** goes upward, X to the right.
 *
 * 2. All bitmaps including the render target are assumed to be bottom-up
 *    (meaning that the first row pointer points to the bottom-left row when
 *    viewed on screen).
 *
 * 3. Unless otherwise specified, all inputs to the renderer are in world coordinate (meters),
 *    NOT pixels. Anything that is in pixel values will be explicitly marked as such.
 *
 * 4. Z is special coordinate because it is broken up into discrete slices,
 *    and the renderer actually understands the slices (potentially).
 *    // TODO(e2dk4r): ZHANDLING
 *
 * 5. All color values specified to the renderer as v4, and they are NOT
 *    premultiplied with alpha.
 */

#include "math.h"
#include "memory_arena.h"
#include "types.h"

#define BITMAP_BYTES_PER_PIXEL 4
struct bitmap {
  u32 alignX;
  u32 alignY;

  u32 width;
  u32 height;
  i32 stride;
  void *memory;
};

struct environment_map {
  struct bitmap lod[4];
  f32 z;
};

struct render_basis {
  struct v3 position;
};

struct render_entity_basis {
  struct render_basis *basis;
  struct v2 offset;
  f32 offsetZ;
};

enum render_group_entry_type {
  RENDER_GROUP_ENTRY_TYPE_CLEAR = 0,
  RENDER_GROUP_ENTRY_TYPE_RECTANGLE = (1 << 0),
  RENDER_GROUP_ENTRY_TYPE_BITMAP = (1 << 1),
  RENDER_GROUP_ENTRY_TYPE_COORDINATE_SYSTEM = (1 << 2),
};

// render_group_entry is tagged union
struct render_group_entry {
  enum render_group_entry_type type : 4;
};

struct render_group_entry_clear {
  struct v4 color;
};

struct render_group_entry_bitmap {
  struct bitmap *bitmap;
  struct render_entity_basis basis;
  f32 alpha;
};

struct render_group_entry_rectangle {
  struct render_entity_basis basis;
  struct v4 color;
  struct v2 dim;
};

struct render_group_entry_coordinate_system {
  struct v2 origin;
  struct v2 xAxis;
  struct v2 yAxis;
  struct v4 color;
  struct bitmap *texture;
  struct bitmap *normalMap;

  struct environment_map *top;
  struct environment_map *middle;
  struct environment_map *bottom;
};

struct render_group {
  struct render_basis *defaultBasis;
  f32 metersToPixels;

  u64 pushBufferTotal;
  u64 pushBufferSize;
  void *pushBufferBase;
};

struct v4
sRGB255toLinear1(struct v4 color);

struct v4
Linear1tosRGB255(struct v4 color);

struct render_group *
RenderGroup(struct memory_arena *arena, u64 pushBufferTotal, f32 metersToPixels);

void
Clear(struct render_group *renderGroup, struct v4 color);

void
Bitmap(struct render_group *renderGroup, struct bitmap *bitmap, struct v2 offset, f32 offsetZ);

void
BitmapWithAlpha(struct render_group *renderGroup, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, f32 alpha);

void
Rect(struct render_group *renderGroup, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color);

void
RectOutline(struct render_group *renderGroup, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color);

void
DrawRectangle(struct bitmap *buffer, struct v2 min, struct v2 max, const struct v4 color);

void
DrawRenderGroup(struct render_group *renderGroup, struct bitmap *outputTarget);

struct render_group_entry_coordinate_system *
CoordinateSystem(struct render_group *renderGroup, struct v2 origin, struct v2 xAxis, struct v2 yAxis, struct v4 color,
                 struct bitmap *texture, struct bitmap *normalMap, struct environment_map *top,
                 struct environment_map *middle, struct environment_map *bottom);

#endif /* HANDMADEHERO_RENDER_GROUP */
