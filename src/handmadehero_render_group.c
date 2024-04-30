#include <handmadehero/handmadehero.h>
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
PushRenderEntry(struct render_group *group, u32 size, enum render_group_entry_type type)
{
  struct render_group_entry *result = 0;

  assert(group->pushBufferSize + size <= group->pushBufferTotal);

  result = group->pushBufferBase + group->pushBufferSize;
  result->type = type;

  group->pushBufferSize += size;

  return result;
}

internal inline void
PushEntryWithZ(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
               struct v2 dim, struct v4 color, f32 z)
{
  struct render_group_entry_rectangle *entry =
      PushRenderEntry(group, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_RECTANGLE);
  entry->basis = group->defaultBasis;
  entry->bitmap = bitmap;
  entry->offset = v2_mul(v2_hadamard(offset, v2(1.0f, -1.0f)), group->metersToPixels);
  entry->offsetZ = offsetZ;
  entry->align = align;
  entry->cZ = z;
  entry->dim = dim;
  entry->color = color;
}

internal inline void
PushEntry(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
          struct v2 dim, struct v4 color)
{
  PushEntryWithZ(group, bitmap, offset, offsetZ, align, dim, color, 1.0f);
}

inline void
PushBitmap(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align)
{
  PushEntry(group, bitmap, offset, offsetZ, align, v2(0.0f, 0.0f), v4(1.0f, 1.0f, 1.0f, 1.0f));
}

inline void
PushBitmapWithAlpha(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
                    f32 alpha)
{
  PushEntry(group, bitmap, offset, offsetZ, align, v2(0.0f, 0.0f), v4(1.0f, 1.0f, 1.0f, alpha));
}

inline void
PushBitmapWithAlphaAndZ(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ,
                        struct v2 align, f32 alpha, f32 z)
{
  PushEntryWithZ(group, bitmap, offset, offsetZ, align, v2(0.0f, 0.0f), v4(1.0f, 1.0f, 1.0f, alpha), z);
}

inline void
PushRect(struct render_group *group, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color)
{
  PushEntry(group, 0, offset, offsetZ, v2(0.0f, 0.0f), dim, color);
}

inline void
PushRectOutline(struct render_group *group, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color)
{
  f32 thickness = 0.1f;

  // NOTE(e2dk4r): top and bottom
  PushEntry(group, 0, v2_sub(offset, v2(0.0f, 0.5f * dim.y)), offsetZ, v2(0.0f, 0.0f), v2(dim.x, thickness), color);
  PushEntry(group, 0, v2_add(offset, v2(0.0f, 0.5f * dim.y)), offsetZ, v2(0.0f, 0.0f), v2(dim.x, thickness), color);

  // NOTE(e2dk4r): left right
  PushEntry(group, 0, v2_sub(offset, v2(0.5f * dim.x, 0.0f)), offsetZ, v2(0.0f, 0.0f), v2(thickness, dim.y), color);
  PushEntry(group, 0, v2_add(offset, v2(0.5f * dim.x, 0.0f)), offsetZ, v2(0.0f, 0.0f), v2(thickness, dim.y), color);
}

inline void
DrawRectangle(struct bitmap *buffer, struct v2 min, struct v2 max, const struct v4 color)
{
  assert(min.x < max.x);
  assert(min.y < max.y);

  i32 minX = roundf32toi32(min.x);
  i32 minY = roundf32toi32(min.y);
  i32 maxX = roundf32toi32(max.x);
  i32 maxY = roundf32toi32(max.y);

  if (minX < 0)
    minX = 0;

  if (minY < 0)
    minY = 0;

  if (maxX > (i32)buffer->width)
    maxX = (i32)buffer->width;

  if (maxY > (i32)buffer->height)
    maxY = (i32)buffer->height;

  u8 *row = buffer->memory
            /* x offset */
            + (minX * BITMAP_BYTES_PER_PIXEL)
            /* y offset */
            + (minY * buffer->stride);

  u32 colorRGBA =
      /* alpha */
      roundf32tou32(color.a * 255.0f) << 24
      /* red */
      | roundf32tou32(color.r * 255.0f) << 16
      /* green */
      | roundf32tou32(color.g * 255.0f) << 8
      /* blue */
      | roundf32tou32(color.b * 255.0f) << 0;

  for (i32 y = minY; y < maxY; y++) {
    u32 *pixel = (u32 *)row;
    for (i32 x = minX; x < maxX; x++) {
      *pixel = colorRGBA;
      pixel++;
    }
    row += buffer->stride;
  }
}

internal inline void
DrawRectangleOutline(struct bitmap *buffer, struct v2 min, struct v2 max, struct v4 color, f32 thickness)
{
  // NOTE(e2dk4r): top and bottom
  DrawRectangle(buffer, v2(min.x - thickness, min.y - thickness), v2(max.x + thickness, min.y + thickness), color);
  DrawRectangle(buffer, v2(min.x - thickness, max.y - thickness), v2(max.x + thickness, max.y + thickness), color);

  // NOTE(e2dk4r): left right
  DrawRectangle(buffer, v2(min.x - thickness, min.y - thickness), v2(min.x + thickness, max.y + thickness), color);
  DrawRectangle(buffer, v2(max.x - thickness, min.y - thickness), v2(max.x + thickness, max.y + thickness), color);
}

internal inline void
DrawBitmapWithAlpha(struct bitmap *buffer, struct bitmap *bitmap, struct v2 pos, struct v2 align, f32 cAlpha)
{
  v2_sub_ref(&pos, align);

  i32 minX = roundf32toi32(pos.x);
  i32 minY = roundf32toi32(pos.y);
  i32 maxX = roundf32toi32(pos.x + (f32)bitmap->width);
  i32 maxY = roundf32toi32(pos.y + (f32)bitmap->height);

  i32 srcOffsetX = 0;
  if (minX < 0) {
    srcOffsetX = -minX;
    minX = 0;
  }

  i32 srcOffsetY = 0;
  if (minY < 0) {
    srcOffsetY = -minY;
    minY = 0;
  }

  if (maxX > (i32)buffer->width)
    maxX = (i32)buffer->width;

  if (maxY > (i32)buffer->height)
    maxY = (i32)buffer->height;

  /* bitmap file pixels goes bottom to up */
  u8 *srcRow = (u8 *)bitmap->memory
               /* last row offset */
               + srcOffsetY * bitmap->stride
               /* clipped offset */
               + srcOffsetX * BITMAP_BYTES_PER_PIXEL;
  u8 *dstRow = (u8 *)buffer->memory
               /* x offset */
               + minX * BITMAP_BYTES_PER_PIXEL
               /* y offset */
               + minY * buffer->stride;
  for (i32 y = minY; y < maxY; y++) {
    u32 *src = (u32 *)srcRow;
    u32 *dst = (u32 *)dstRow;

    for (i32 x = minX; x < maxX; x++) {
      // source channels
      f32 sA = (f32)((*src >> 24) & 0xff);
      f32 sR = cAlpha * (f32)((*src >> 16) & 0xff);
      f32 sG = cAlpha * (f32)((*src >> 8) & 0xff);
      f32 sB = cAlpha * (f32)((*src >> 0) & 0xff);

      // normalized sA
      f32 nsA = (sA / 255.0f) * cAlpha;

      // destination channels
      f32 dA = (f32)((*dst >> 24) & 0xff);
      f32 dR = (f32)((*dst >> 16) & 0xff);
      f32 dG = (f32)((*dst >> 8) & 0xff);
      f32 dB = (f32)((*dst >> 0) & 0xff);

      // normalized dA
      f32 ndA = (dA / 255.0f);

      // percentage of normalized sA to be applied
      f32 psA = (1.0f - nsA);

      /*
       * Math of calculating blended alpha
       * videoId:   bidrZj1YosA
       * timestamp: 01:06:19
       */
      f32 a = 255.0f * (nsA + ndA - nsA * ndA);
      f32 r = psA * dR + sR;
      f32 g = psA * dG + sG;
      f32 b = psA * dB + sB;

      *dst =
          /* alpha */
          (u32)(a + 0.5f) << 24
          /* red */
          | (u32)(r + 0.5f) << 16
          /* green */
          | (u32)(g + 0.5f) << 8
          /* blue */
          | (u32)(b + 0.5f) << 0;

      dst++;
      src++;
    }

    dstRow += buffer->stride;
    srcRow += bitmap->stride;
  }
}

inline void
DrawBitmap(struct bitmap *buffer, struct bitmap *bitmap, struct v2 pos, struct v2 align)
{
  DrawBitmapWithAlpha(buffer, bitmap, pos, align, 1.0f);
}

inline void
DrawRenderGroup(struct render_group *renderGroup, struct bitmap *outputTarget)
{
  f32 metersToPixels = renderGroup->metersToPixels;
  struct v2 screenCenter = v2_mul(v2u(outputTarget->width, outputTarget->height), 0.5f);

  for (u32 pushBufferIndex = 0; pushBufferIndex < renderGroup->pushBufferSize;) {
    struct render_group_entry *typelessEntry = renderGroup->pushBufferBase + pushBufferIndex;
    if (typelessEntry->type == RENDER_GROUP_ENTRY_TYPE_CLEAR) {
      struct render_group_entry_clear *entry = (struct render_group_entry_clear *)typelessEntry;
      pushBufferIndex += sizeof(*entry);
    } else if (typelessEntry->type & RENDER_GROUP_ENTRY_TYPE_RECTANGLE) {
      struct render_group_entry_rectangle *entry = (struct render_group_entry_rectangle *)typelessEntry;
      pushBufferIndex += sizeof(*entry);

      struct v3 entityBasePosition = entry->basis->position;

      entityBasePosition.y += entry->offsetZ;
      /* screen's coordinate system uses y values inverse,
       * so that means going up in space means negative y values
       */
      entityBasePosition.y *= -1;
      v2_mul_ref(&entityBasePosition.xy, metersToPixels);

      f32 entityZ = -entityBasePosition.z * metersToPixels;

      struct v2 entityGroundPoint = v2_add(screenCenter, entityBasePosition.xy);
      struct v2 center = v2_add(entityGroundPoint, entry->offset);
      center.y += entry->cZ * entityZ;

      if (entry->bitmap) {
        DrawBitmapWithAlpha(outputTarget, entry->bitmap, center, entry->align, entry->color.a);
      } else {
        struct v2 halfDim = v2_mul(v2_mul(entry->dim, 0.5f), metersToPixels);
        DrawRectangle(outputTarget, v2_sub(center, halfDim), v2_add(center, halfDim), entry->color);
      }
    } else {
      assert(0 && "this renderer does not know how to handle render group entry type");
    }
  }
}
