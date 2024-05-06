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
  void *data = 0;
  struct render_group_entry *header;

  size += sizeof(*header);

  assert(group->pushBufferSize + size <= group->pushBufferTotal);

  header = group->pushBufferBase + group->pushBufferSize;
  header->type = type;
  data = (u8 *)header + sizeof(*header);

  group->pushBufferSize += size;

  return data;
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
                 struct bitmap *texture, struct bitmap *normalMap, struct environment_map *top,
                 struct environment_map *middle, struct environment_map *bottom)
{
  struct render_group_entry_coordinate_system *entry =
      PushRenderEntry(group, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_COORDINATE_SYSTEM);
  entry->origin = origin;
  entry->xAxis = xAxis;
  entry->yAxis = yAxis;
  entry->color = color;
  entry->texture = texture;
  entry->normalMap = normalMap;
  entry->top = top;
  entry->middle = middle;
  entry->bottom = bottom;
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

inline struct v4
sRGB255toLinear1(struct v4 color)
{
  struct v4 result;

  f32 inv255 = 1.0f / 255.0f;
  result.r = Square(inv255 * color.r);
  result.g = Square(inv255 * color.g);
  result.b = Square(inv255 * color.b);
  result.a = inv255 * color.a;

  return result;
}

inline struct v4
Linear1tosRGB255(struct v4 color)
{
  struct v4 result;

  result.r = 255.0f * SquareRoot(color.r);
  result.g = 255.0f * SquareRoot(color.g);
  result.b = 255.0f * SquareRoot(color.b);
  result.a = 255.0f * color.a;

  return result;
}

internal inline struct v4
Unpack4x8(u32 *pixel)
{
  return v4((f32)((*pixel >> 0x10) & 0xff), (f32)((*pixel >> 0x08) & 0xff), (f32)((*pixel >> 0x00) & 0xff),
            (f32)((*pixel >> 0x18) & 0xff));
}

struct bilinear_sample {
  u32 *a;
  u32 *b;
  u32 *c;
  u32 *d;
};

internal inline struct bilinear_sample
BilinearSample(struct bitmap *texture, i32 x, i32 y)
{
  struct bilinear_sample result;

  result.a = (u32 *)((u8 *)texture->memory + y * texture->stride + x * BITMAP_BYTES_PER_PIXEL);
  result.b = (u32 *)((u8 *)result.a + BITMAP_BYTES_PER_PIXEL);
  result.c = (u32 *)((u8 *)result.a + texture->stride);
  result.d = (u32 *)((u8 *)result.c + BITMAP_BYTES_PER_PIXEL);

  return result;
}

internal inline struct v4
sRGBBilinearBlend(struct bilinear_sample texelSample, f32 fX, f32 fY)
{
  struct v4 texelA = Unpack4x8(texelSample.a);
  struct v4 texelB = Unpack4x8(texelSample.b);
  struct v4 texelC = Unpack4x8(texelSample.c);
  struct v4 texelD = Unpack4x8(texelSample.d);

  // NOTE(e2dk4r): Go from sRGB to "linear" brightness space
  texelA = sRGB255toLinear1(texelA);
  texelB = sRGB255toLinear1(texelB);
  texelC = sRGB255toLinear1(texelC);
  texelD = sRGB255toLinear1(texelD);

  struct v4 blend = v4_lerp(v4_lerp(texelA, texelB, fX), v4_lerp(texelC, texelD, fX), fY);
  return blend;
}

internal inline struct v4
UnscaleAndBiasNormal(struct v4 normal)
{
  struct v4 result;

  result = v4_mul(normal, 1.0f / 255.0f);
  result.xyz = v3_sub(v3_mul(result.xyz, 2.0f), v3(1.0f, 1.0f, 1.0f));

  return result;
}

internal inline struct v3
SampleEnvironmentMap(struct environment_map *map, struct v2 screenSpaceUV, struct v3 normal, f32 roughness)
{

  u32 lodIndex = (u32)(roughness * (f32)(ARRAY_COUNT(map->lod) - 1) + 0.5f);
  assert(lodIndex < ARRAY_COUNT(map->lod));

  struct bitmap *lod = map->lod + lodIndex;

  // TODO(e2dk4r): Do intersection math to determine where we should be!
  f32 tX = 0.0f;
  f32 tY = 0.0f;

  i32 x = (i32)tX;
  i32 y = (i32)tY;

  f32 fX = tX - (f32)x;
  f32 fY = tY - (f32)y;

  assert(x >= 0 && x < (i32)lod->width);
  assert(y >= 0 && y < (i32)lod->height);

  struct bilinear_sample sample = BilinearSample(lod, x, y);
  struct v3 result = sRGBBilinearBlend(sample, fX, fY).xyz;

  return result;
}

internal inline void
DrawRectangleSlowly(struct bitmap *buffer, struct v2 origin, struct v2 xAxis, struct v2 yAxis, struct v4 color,
                    struct bitmap *texture, struct bitmap *normalMap, struct environment_map *top,
                    struct environment_map *middle, struct environment_map *bottom)
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

  f32 invWidthMax = 1.0f / (f32)widthMax;
  f32 invHeightMax = 1.0f / (f32)heightMax;

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

  // pre-multiplied alpha
  v3_mul_ref(&color.rgb, color.a);
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
        struct v2 screenSpaceUV = v2(pixelP.x * invWidthMax, pixelP.y * invHeightMax);
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
        struct bilinear_sample texelSample = BilinearSample(texture, texelX, texelY);
        struct v4 texel = sRGBBilinearBlend(texelSample, fX, fY);

        if (normalMap) {
          struct bilinear_sample normalSample = BilinearSample(normalMap, texelX, texelY);

          struct v4 normalA = Unpack4x8(normalSample.a);
          struct v4 normalB = Unpack4x8(normalSample.b);
          struct v4 normalC = Unpack4x8(normalSample.c);
          struct v4 normalD = Unpack4x8(normalSample.d);

          struct v4 normal = v4_lerp(v4_lerp(normalA, normalB, fX), v4_lerp(normalC, normalD, fX), fY);

          normal = UnscaleAndBiasNormal(normal);
          // TODO(e2dk4r): do we need to do this?
          normal.xyz = v3_normalize(normal.xyz);

          f32 tEnvMap = normal.y;
          f32 tFarMap = 0.0f;
          struct environment_map *farMap = 0;
          if (tEnvMap < -0.5f) {
            farMap = bottom;
            tFarMap = -1.0f - 2.0f * tEnvMap;
          } else if (tEnvMap > 0.5f) {
            farMap = top;
            tFarMap = 2.0f * (tEnvMap - 0.5f);
          }

          struct v3 lightColor =
              v3(0.0f, 0.0f, 0.0f); // SampleEnvironmentMap(middle, screenSpaceUV, normal.xyz, normal.w);
          if (farMap) {
            struct v3 farMapColor = SampleEnvironmentMap(farMap, screenSpaceUV, normal.xyz, normal.w);
            lightColor = v3_lerp(lightColor, farMapColor, tFarMap);
          }

          v3_add_ref(&texel.xyz, v3_mul(lightColor, texel.a));
        }

        texel = v4_hadamard(texel, color);
        texel.r = Clamp01(texel.r);
        texel.g = Clamp01(texel.g);
        texel.b = Clamp01(texel.b);

        // destination channels
        struct v4 dest = Unpack4x8(pixel);
        // NOTE(e2dk4r): Go from sRGB to "linear" brightness space
        dest = sRGB255toLinear1(dest);

        // blend alpha
        struct v4 blended = v4_add(v4_mul(dest, 1.0f - texel.a), texel);

        // NOTE(e2dk4r): Go from "linear" brightness space to sRGB
        blended = Linear1tosRGB255(blended);

        *pixel = (u32)(blended.a + 0.5f) << 0x18 | (u32)(blended.r + 0.5f) << 0x10 | (u32)(blended.g + 0.5f) << 0x08 |
                 (u32)(blended.b + 0.5f) << 0x00;
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
      struct v4 texel = v4((f32)((*src >> 0x10) & 0xff), (f32)((*src >> 0x08) & 0xff), (f32)((*src >> 0x00) & 0xff),
                           (f32)((*src >> 0x18) & 0xff));
      texel = sRGB255toLinear1(texel);
      v3_mul_ref(&texel.rgb, cAlpha);

      // destination channels
      struct v4 d = v4((f32)((*dst >> 0x10) & 0xff), (f32)((*dst >> 0x08) & 0xff), (f32)((*dst >> 0x00) & 0xff),
                       (f32)((*dst >> 0x18) & 0xff));
      d = sRGB255toLinear1(d);

      /*
       * Math of calculating blended alpha
       * videoId:   bidrZj1YosA
       * timestamp: 01:06:19
       */
      struct v4 blended = v4_add(v4_mul(d, 1.0f - texel.a), texel);

      blended = Linear1tosRGB255(blended);
      *dst = (u32)(blended.a + 0.5f) << 0x18 | (u32)(blended.r + 0.5f) << 0x10 | (u32)(blended.g + 0.5f) << 0x08 |
             (u32)(blended.b + 0.5f) << 0x00;

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
    pushBufferIndex += sizeof(*header);
    void *data = (u8 *)header + sizeof(*header);

    if (header->type == RENDER_GROUP_ENTRY_TYPE_CLEAR) {
      struct render_group_entry_clear *entry = (struct render_group_entry_clear *)data;
      pushBufferIndex += sizeof(*entry);

      DrawRectangle(outputTarget, v2(0.0f, 0.0f), screenWidthHeight, entry->color);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_BITMAP) {
      struct render_group_entry_bitmap *entry = (struct render_group_entry_bitmap *)data;
      pushBufferIndex += sizeof(*entry);

      // assert(entry->bitmap);
      // struct v2 center = GetEntityCenter(renderGroup, &entry->basis, screenCenter);
      // DrawBitmapWithAlpha(outputTarget, entry->bitmap, center, entry->align, entry->alpha);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_RECTANGLE) {
      struct render_group_entry_rectangle *entry = (struct render_group_entry_rectangle *)data;
      pushBufferIndex += sizeof(*entry);

      struct v2 center = GetEntityCenter(renderGroup, &entry->basis, screenCenter);
      struct v2 halfDim = v2_mul(v2_mul(entry->dim, 0.5f), metersToPixels);
      DrawRectangle(outputTarget, v2_sub(center, halfDim), v2_add(center, halfDim), entry->color);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_COORDINATE_SYSTEM) {
      struct render_group_entry_coordinate_system *entry = (struct render_group_entry_coordinate_system *)data;
      pushBufferIndex += sizeof(*entry);

      DrawRectangleSlowly(outputTarget, entry->origin, entry->xAxis, entry->yAxis, entry->color, entry->texture,
                          entry->normalMap, entry->top, entry->middle, entry->bottom);

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
    }

    // typelessEntry->type is invalid
    else {
      assert(0 && "this renderer does not know how to handle render group entry type");
    }
  }
}
