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
PushClearEntry(struct render_group *group, struct v4 color)
{
  struct render_group_entry_clear *entry = PushRenderEntry(group, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_CLEAR);
  entry->color = color;
}

internal inline struct v2
v2_screen_coordinates(struct v2 a)
{
  return v2(a.x, -a.y);
}

internal inline struct v3
v3_screen_coordinates(struct v3 a)
{
  return v3(a.x, -a.y, a.z);
}

internal inline void
PushBitmapEntry(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
                f32 alpha, f32 z)
{
  struct render_group_entry_bitmap *entry = PushRenderEntry(group, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_BITMAP);
  entry->bitmap = bitmap;
  entry->align = align;
  entry->basis.basis = group->defaultBasis;
  entry->basis.offset = v2_mul(v2_screen_coordinates(offset), group->metersToPixels);
  entry->basis.offsetZ = offsetZ;
  entry->basis.cZ = z;
  entry->alpha = alpha;
}

internal inline void
PushRectangleEntry(struct render_group *group, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color)
{
  struct render_group_entry_rectangle *entry =
      PushRenderEntry(group, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_RECTANGLE);
  entry->basis.basis = group->defaultBasis;
  entry->basis.offset = v2_mul(v2_screen_coordinates(offset), group->metersToPixels);
  entry->basis.offsetZ = offsetZ;
  entry->basis.cZ = 1.0f;
  entry->dim = dim;
  entry->color = color;
}

struct render_group_entry_coordinate_system *
CoordinateSystem(struct render_group *group, struct v2 origin, struct v2 xAxis, struct v2 yAxis, struct v4 color,
                 struct bitmap *texture)
{
  struct render_group_entry_coordinate_system *entry =
      PushRenderEntry(group, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_COORDINATE_SYSTEM);
  entry->origin = origin;
  entry->xAxis = xAxis;
  entry->yAxis = yAxis;
  entry->color = color;
  entry->texture = texture;
  return entry;
}

void
PushClear(struct render_group *group, struct v4 color)
{
  PushClearEntry(group, color);
}

inline void
PushBitmap(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align)
{
  PushBitmapEntry(group, bitmap, offset, offsetZ, align, 1.0f, 1.0f);
}

inline void
PushBitmapWithAlpha(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ, struct v2 align,
                    f32 alpha)
{
  PushBitmapEntry(group, bitmap, offset, offsetZ, align, alpha, 1.0f);
}

inline void
PushBitmapWithAlphaAndZ(struct render_group *group, struct bitmap *bitmap, struct v2 offset, f32 offsetZ,
                        struct v2 align, f32 alpha, f32 z)
{
  PushBitmapEntry(group, bitmap, offset, offsetZ, align, alpha, z);
}

inline void
PushRect(struct render_group *group, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color)
{
  PushRectangleEntry(group, offset, offsetZ, dim, color);
}

inline void
PushRectOutline(struct render_group *group, struct v2 offset, f32 offsetZ, struct v2 dim, struct v4 color)
{
  f32 thickness = 0.1f;

  // NOTE(e2dk4r): top and bottom
  PushRectangleEntry(group, v2_sub(offset, v2(0.0f, 0.5f * dim.y)), offsetZ, v2(dim.x, thickness), color);
  PushRectangleEntry(group, v2_add(offset, v2(0.0f, 0.5f * dim.y)), offsetZ, v2(dim.x, thickness), color);

  // NOTE(e2dk4r): left right
  PushRectangleEntry(group, v2_sub(offset, v2(0.5f * dim.x, 0.0f)), offsetZ, v2(thickness, dim.y), color);
  PushRectangleEntry(group, v2_add(offset, v2(0.5f * dim.x, 0.0f)), offsetZ, v2(thickness, dim.y), color);
}

internal inline void
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

internal inline struct v4
sRGB255toLinear1(struct v4 color)
{
  struct v4 result;

  f32 inv255 = 1.0f / 255.0f;
  result.r = square(inv255 * color.r);
  result.g = square(inv255 * color.g);
  result.b = square(inv255 * color.b);
  result.a = inv255 * color.a;

  return result;
}

internal inline struct v4
Linear1tosRGB255(struct v4 color)
{
  struct v4 result;

  result.r = 255.0f * SquareRoot(color.r);
  result.g = 255.0f * SquareRoot(color.g);
  result.b = 255.0f * SquareRoot(color.b);
  result.a = 255.0f * color.a;

  return result;
}

internal inline void
DrawRectangleSlowly(struct bitmap *buffer, struct v2 origin, struct v2 xAxis, struct v2 yAxis, const struct v4 color,
                    struct bitmap *texture)
{
  f32 InvXAxisLengthSq = 1.0f / v2_length_square(xAxis);
  f32 InvYAxisLengthSq = 1.0f / v2_length_square(yAxis);

  struct v2 p[4] = {
      origin,
      v2_add(origin, xAxis),
      v2_add(origin, v2_add(xAxis, yAxis)),
      v2_add(origin, yAxis),
  };

  i32 widthMax = (i32)buffer->width - 1;
  i32 heightMax = (i32)buffer->height - 1;

  i32 xMin = widthMax;
  i32 xMax = 0;
  i32 yMin = heightMax;
  i32 yMax = 0;

  for (u32 pIndex = 0; pIndex < ARRAY_COUNT(p); pIndex++) {
    struct v2 testP = p[pIndex];
    i32 floorX = Floor(testP.x);
    i32 ceilX = Ceil(testP.x);
    i32 floorY = Floor(testP.y);
    i32 ceilY = Ceil(testP.y);

    if (xMin > floorX)
      xMin = floorX;

    if (xMax < ceilX)
      xMax = ceilX;

    if (yMin > floorY)
      yMin = floorY;

    if (yMax < ceilY)
      yMax = ceilY;
  }

  if (xMin < 0)
    xMin = 0;
  if (xMax > widthMax)
    xMax = widthMax;

  if (yMin < 0)
    yMin = 0;
  if (yMax > heightMax)
    yMax = heightMax;

  u8 *row = buffer->memory + yMin * buffer->stride + xMin * BITMAP_BYTES_PER_PIXEL;

  u32 colorRGBA =
      /* alpha */
      roundf32tou32(color.a * 255.0f) << 24
      /* red */
      | roundf32tou32(color.r * 255.0f) << 16
      /* green */
      | roundf32tou32(color.g * 255.0f) << 8
      /* blue */
      | roundf32tou32(color.b * 255.0f) << 0;

  for (i32 y = yMin; y <= yMax; y++) {
    u32 *pixel = (u32 *)row;
    for (i32 x = xMin; x <= xMax; x++) {
#if 1
      struct v2 pixelP = v2i(x, y);
      struct v2 d = v2_sub(pixelP, origin);
      // TODO(e2dk4r): PerpDot()
      // TODO(e2dk4r): Simpler origin
      f32 edge0 = v2_dot(d, v2_neg(v2_perp(xAxis)));
      f32 edge1 = v2_dot(v2_sub(d, xAxis), v2_neg(v2_perp(yAxis)));
      f32 edge2 = v2_dot(v2_sub(d, v2_add(xAxis, yAxis)), v2_perp(xAxis));
      f32 edge3 = v2_dot(v2_sub(d, yAxis), v2_perp(yAxis));

      if (edge0 < 0 && edge1 < 0 && edge2 < 0 && edge3 < 0) {
        f32 u = InvXAxisLengthSq * v2_dot(d, xAxis);
        f32 v = InvYAxisLengthSq * v2_dot(d, yAxis);
        assert(u > 0.0f && v > 0.0f);

        f32 tX = u * (f32)(texture->width - 2);
        f32 tY = v * (f32)(texture->height - 2);

        i32 texelX = (i32)tX;
        i32 texelY = (i32)tY;

        f32 fX = tX - (f32)texelX;
        f32 fY = tY - (f32)texelY;

        assert(texelX >= 0 && texelX < (i32)texture->width);
        assert(texelY >= 0 && texelY < (i32)texture->height);

        /*
         * Texture filtering, bilinear filtering
         * | A | B | ...
         * | C | D | ...
         */
        u32 *texelPtrA = (u32 *)((u8 *)texture->memory + texelY * texture->stride + texelX * BITMAP_BYTES_PER_PIXEL);
        u32 *texelPtrB = (u32 *)((u8 *)texelPtrA + BITMAP_BYTES_PER_PIXEL);
        u32 *texelPtrC = (u32 *)((u8 *)texelPtrA + texture->stride);
        u32 *texelPtrD = (u32 *)((u8 *)texelPtrC + BITMAP_BYTES_PER_PIXEL);

        struct v4 texelA = v4((f32)((*texelPtrA >> 0x10) & 0xff), (f32)((*texelPtrA >> 0x08) & 0xff),
                              (f32)((*texelPtrA >> 0x00) & 0xff), (f32)((*texelPtrA >> 0x18) & 0xff));
        struct v4 texelB = v4((f32)((*texelPtrB >> 0x10) & 0xff), (f32)((*texelPtrB >> 0x08) & 0xff),
                              (f32)((*texelPtrB >> 0x00) & 0xff), (f32)((*texelPtrB >> 0x18) & 0xff));
        struct v4 texelC = v4((f32)((*texelPtrC >> 0x10) & 0xff), (f32)((*texelPtrC >> 0x08) & 0xff),
                              (f32)((*texelPtrC >> 0x00) & 0xff), (f32)((*texelPtrC >> 0x18) & 0xff));
        struct v4 texelD = v4((f32)((*texelPtrD >> 0x10) & 0xff), (f32)((*texelPtrD >> 0x08) & 0xff),
                              (f32)((*texelPtrD >> 0x00) & 0xff), (f32)((*texelPtrD >> 0x18) & 0xff));

        // NOTE(e2dk4r): Go from sRGB to "linear" brightness space
        texelA = sRGB255toLinear1(texelA);
        texelB = sRGB255toLinear1(texelB);
        texelC = sRGB255toLinear1(texelC);
        texelD = sRGB255toLinear1(texelD);

        struct v4 texel = v4_lerp(v4_lerp(texelA, texelB, fX), v4_lerp(texelC, texelD, fX), fY);
        f32 nsA = texel.a * color.a;

        // destination channels
        struct v4 dest = v4((f32)((*pixel >> 0x10) & 0xff), (f32)((*pixel >> 0x08) & 0xff),
                            (f32)((*pixel >> 0x00) & 0xff), (f32)((*pixel >> 0x18) & 0xff));
        // NOTE(e2dk4r): Go from sRGB to "linear" brightness space
        dest = sRGB255toLinear1(dest);

        // percentage of normalized sA to be applied
        f32 psA = 1.0f - nsA;

        // blend alpha
        struct v4 blended = v4(psA * dest.r + color.r * texel.r, psA * dest.g + color.g * texel.g,
                               psA * dest.b + color.b * texel.b, nsA + dest.a - nsA * dest.a);

        // NOTE(e2dk4r): Go from "linear" brightness space to sRGB
        struct v4 blended255 = Linear1tosRGB255(blended);

        *pixel = (u32)(blended255.a + 0.5f) << 0x18 | (u32)(blended255.r + 0.5f) << 0x10 |
                 (u32)(blended255.g + 0.5f) << 0x08 | (u32)(blended255.b + 0.5f) << 0x00;
      }
#else
      *pixel = colorRGBA;
#endif

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

internal inline void
DrawBitmap(struct bitmap *buffer, struct bitmap *bitmap, struct v2 pos, struct v2 align)
{
  DrawBitmapWithAlpha(buffer, bitmap, pos, align, 1.0f);
}

internal struct v2
GetEntityCenter(struct render_group *renderGroup, struct render_entity_basis *entityBasis, struct v2 screenCenter)
{
  f32 metersToPixels = renderGroup->metersToPixels;
  struct v3 entityBasePosition = entityBasis->basis->position;

  entityBasePosition.y += entityBasis->offsetZ;
  /* screen's coordinate system uses y values inverse,
   * so that means going up in space means negative y values
   */
  entityBasePosition.y *= -1;
  v2_mul_ref(&entityBasePosition.xy, metersToPixels);

  f32 entityZ = -entityBasePosition.z * metersToPixels;

  struct v2 entityGroundPoint = v2_add(screenCenter, entityBasePosition.xy);
  struct v2 center = v2_add(entityGroundPoint, entityBasis->offset);
  center.y += entityBasis->cZ * entityZ;

  return center;
}

inline void
DrawRenderGroup(struct render_group *renderGroup, struct bitmap *outputTarget)
{
  f32 metersToPixels = renderGroup->metersToPixels;
  struct v2 screenWidthHeight = v2u(outputTarget->width, outputTarget->height);
  struct v2 screenCenter = v2_mul(screenWidthHeight, 0.5f);

  for (u32 pushBufferIndex = 0; pushBufferIndex < renderGroup->pushBufferSize;) {
    struct render_group_entry *header = renderGroup->pushBufferBase + pushBufferIndex;

    if (header->type == RENDER_GROUP_ENTRY_TYPE_CLEAR) {
      struct render_group_entry_clear *entry = (struct render_group_entry_clear *)header;
      pushBufferIndex += sizeof(*entry);

      DrawRectangle(outputTarget, v2(0.0f, 0.0f), screenWidthHeight, entry->color);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_BITMAP) {
      struct render_group_entry_bitmap *entry = (struct render_group_entry_bitmap *)header;
      pushBufferIndex += sizeof(*entry);

      assert(entry->bitmap);
      struct v2 center = GetEntityCenter(renderGroup, &entry->basis, screenCenter);
      DrawBitmapWithAlpha(outputTarget, entry->bitmap, center, entry->align, entry->alpha);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_RECTANGLE) {
      struct render_group_entry_rectangle *entry = (struct render_group_entry_rectangle *)header;
      pushBufferIndex += sizeof(*entry);

      struct v2 center = GetEntityCenter(renderGroup, &entry->basis, screenCenter);
      struct v2 halfDim = v2_mul(v2_mul(entry->dim, 0.5f), metersToPixels);
      DrawRectangle(outputTarget, v2_sub(center, halfDim), v2_add(center, halfDim), entry->color);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_COORDINATE_SYSTEM) {
      struct render_group_entry_coordinate_system *entry = (struct render_group_entry_coordinate_system *)header;
      pushBufferIndex += sizeof(*entry);

      DrawRectangleSlowly(outputTarget, entry->origin, entry->xAxis, entry->yAxis, entry->color, entry->texture);

      struct v4 color = v4(1.0f, 0.0f, 0.0f, 1.0f);
      struct v2 dim = v2(2.0f, 2.0f);
      struct v2 p = entry->origin;
      DrawRectangle(outputTarget, v2_sub(p, dim), v2_add(p, dim), color);

      p = v2_add(entry->origin, entry->xAxis);
      DrawRectangle(outputTarget, v2_sub(p, dim), v2_add(p, dim), color);

      p = v2_add(entry->origin, entry->yAxis);
      DrawRectangle(outputTarget, v2_sub(p, dim), v2_add(p, dim), color);

      p = v2_add(entry->origin, v2_add(entry->xAxis, entry->yAxis));
      DrawRectangle(outputTarget, v2_sub(p, dim), v2_add(p, dim), color);

#if 0
      for (u32 pIndex = 0; pIndex < ARRAY_COUNT(entry->points); pIndex++) {
        p = entry->points[pIndex];
        p = v2_add(entry->origin, v2_add(v2_mul(entry->xAxis, p.x), v2_mul(entry->yAxis, p.y)));
        DrawRectangle(outputTarget, v2_sub(p, dim), v2_add(p, dim), entry->color);
      }
#endif
    }

    // typelessEntry->type is invalid
    else {
      assert(0 && "this renderer does not know how to handle render group entry type");
    }
  }
}
