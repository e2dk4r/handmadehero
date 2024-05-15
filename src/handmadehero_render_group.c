#include <handmadehero/handmadehero.h>
#include <handmadehero/render_group.h>

struct render_group *
RenderGroup(struct memory_arena *arena, u64 pushBufferTotal, f32 metersToPixels)
{
  struct render_group *renderGroup = MemoryArenaPush(arena, sizeof(*renderGroup));

  renderGroup->defaultBasis = MemoryArenaPush(arena, sizeof(*renderGroup->defaultBasis));
  renderGroup->defaultBasis->position = v3(0.0f, 0.0f, 0.0f);

  renderGroup->metersToPixels = metersToPixels;
  renderGroup->alpha = 1.0f;

  renderGroup->pushBufferSize = 0;
  renderGroup->pushBufferTotal = pushBufferTotal;
  renderGroup->pushBufferBase = MemoryArenaPush(arena, renderGroup->pushBufferTotal);

  return renderGroup;
}

internal inline void *
PushRenderEntry(struct render_group *renderGroup, u32 size, enum render_group_entry_type type)
{
  void *data = 0;
  struct render_group_entry *header;

  size += sizeof(*header);

  assert(renderGroup->pushBufferSize + size <= renderGroup->pushBufferTotal);

  header = renderGroup->pushBufferBase + renderGroup->pushBufferSize;
  header->type = type;
  data = (u8 *)header + sizeof(*header);

  renderGroup->pushBufferSize += size;

  return data;
}

internal inline void
PushClearEntry(struct render_group *renderGroup, struct v4 color)
{
  struct render_group_entry_clear *entry = PushRenderEntry(renderGroup, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_CLEAR);
  entry->color = color;
}

internal inline void
PushBitmapEntry(struct render_group *renderGroup, struct bitmap *bitmap, struct v3 offset, f32 alpha)
{
  struct render_group_entry_bitmap *entry =
      PushRenderEntry(renderGroup, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_BITMAP);
  entry->bitmap = bitmap;
  entry->basis.basis = renderGroup->defaultBasis;

  entry->basis.offset = v3_mul(offset, renderGroup->metersToPixels);
  struct v2 alignPixel = v2u(bitmap->alignX, bitmap->alignY);
  v2_sub_ref(&entry->basis.offset.xy, alignPixel);

  entry->alpha = alpha;
}

internal inline void
PushRectangleEntry(struct render_group *renderGroup, struct v3 offset, struct v2 dim, struct v4 color)
{
  struct render_group_entry_rectangle *entry =
      PushRenderEntry(renderGroup, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_RECTANGLE);
  entry->basis.basis = renderGroup->defaultBasis;
  entry->basis.offset = v3_mul(offset, renderGroup->metersToPixels);
  entry->dim = dim;
  entry->color = color;
}

struct render_group_entry_coordinate_system *
CoordinateSystem(struct render_group *renderGroup, struct v2 origin, struct v2 xAxis, struct v2 yAxis, struct v4 color,
                 struct bitmap *texture, struct bitmap *normalMap, struct environment_map *top,
                 struct environment_map *middle, struct environment_map *bottom)
{
  struct render_group_entry_coordinate_system *entry =
      PushRenderEntry(renderGroup, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_COORDINATE_SYSTEM);
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
Clear(struct render_group *renderGroup, struct v4 color)
{
  PushClearEntry(renderGroup, color);
}

inline void
Bitmap(struct render_group *renderGroup, struct bitmap *bitmap, struct v3 offset)
{
  PushBitmapEntry(renderGroup, bitmap, offset, renderGroup->alpha * 1.0f);
}

inline void
BitmapWithAlpha(struct render_group *renderGroup, struct bitmap *bitmap, struct v3 offset, f32 alpha)
{
  PushBitmapEntry(renderGroup, bitmap, offset, renderGroup->alpha * alpha);
}

inline void
Rect(struct render_group *renderGroup, struct v3 offset, struct v2 dim, struct v4 color)
{
  PushRectangleEntry(renderGroup, offset, dim, color);
}

inline void
RectOutline(struct render_group *renderGroup, struct v3 offset, struct v2 dim, struct v4 color)
{
  f32 thickness = 0.1f;

  // NOTE(e2dk4r): top and bottom
  PushRectangleEntry(renderGroup, v3_sub(offset, v3(0.0f, 0.5f * dim.y, 0.0f)), v2(dim.x, thickness), color);
  PushRectangleEntry(renderGroup, v3_add(offset, v3(0.0f, 0.5f * dim.y, 0.0f)), v2(dim.x, thickness), color);

  // NOTE(e2dk4r): left right
  PushRectangleEntry(renderGroup, v3_sub(offset, v3(0.5f * dim.x, 0.0f, 0.0f)), v2(thickness, dim.y), color);
  PushRectangleEntry(renderGroup, v3_add(offset, v3(0.5f * dim.x, 0.0f, 0.0f)), v2(thickness, dim.y), color);
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

internal inline void
Pack4x8(u32 *dest, struct v4 color)
{
  *dest = (u32)(color.a + 0.5f) << 0x18 | (u32)(color.r + 0.5f) << 0x10 | (u32)(color.g + 0.5f) << 0x08 |
          (u32)(color.b + 0.5f) << 0x00;
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
SampleEnvironmentMap(struct environment_map *map, struct v2 screenSpaceUV, struct v3 sampleDirection, f32 roughness,
                     f32 distanceFromMapInZ)
{
  /* NOTE(e2dk4r):
   *
   *  screenSpaceUV tells us where the ray is being cast from
   *  in normalized space coordinates.
   *
   *  sampleDirection tells us what direction the cast is going.
   *  It does not have to be normalized. But its y *must* be positive.
   *
   *  roughness tells us which LODs of map we sample from.
   *
   */

  // NOTE(e2dk4r): pick which LOD to sample from
  u32 lodIndex = (u32)(roughness * (f32)(ARRAY_COUNT(map->lod) - 1) + 0.5f);
  assert(lodIndex < ARRAY_COUNT(map->lod));

  // NOTE(e2dk4r): compute the distance to the map
  f32 uvPerMeter = 0.05f;
  f32 c = uvPerMeter * distanceFromMapInZ / sampleDirection.y;
  // TODO(e2dk4r): make sure we know what direction Z should go in Y
  struct v2 offset = v2_mul(v2(sampleDirection.x, sampleDirection.z), c);

  // NOTE(e2dk4r): find the intersection point
  struct v2 uv = v2_add(screenSpaceUV, offset);

  // NOTE(e2dk4r): clamp to the valid range
  uv.x = Clamp01(uv.x);
  uv.y = Clamp01(uv.y);

  // NOTE(e2dk4r): bilinear sample
  struct bitmap *lod = map->lod + lodIndex;

  f32 tX = uv.x * (f32)(lod->width - 2);
  f32 tY = uv.y * (f32)(lod->height - 2);

  i32 x = (i32)tX;
  i32 y = (i32)tY;

  f32 fX = tX - (f32)x;
  f32 fY = tY - (f32)y;

  assert(x >= 0 && x < (i32)lod->width);
  assert(y >= 0 && y < (i32)lod->height);

#if 0
  // NOTE(e2dk4r): turn this on to see where in the world you are sampling
  u8 *lodTexel = (u8 *)lod->memory + y * lod->stride + x * BITMAP_BYTES_PER_PIXEL;
  *(u32 *)lodTexel = 0xffffffff;
#endif

  struct bilinear_sample sample = BilinearSample(lod, x, y);
  struct v3 result = sRGBBilinearBlend(sample, fX, fY).xyz;

  return result;
}

internal inline void
DrawRectangleSlowly(struct bitmap *buffer, struct v2 origin, struct v2 xAxis, struct v2 yAxis, struct v4 color,
                    struct bitmap *texture, struct bitmap *normalMap, struct environment_map *top,
                    struct environment_map *middle, struct environment_map *bottom, f32 pixelsToMeters)
{
  f32 InvXAxisLengthSq = 1.0f / v2_length_square(xAxis);
  f32 InvYAxisLengthSq = 1.0f / v2_length_square(yAxis);

  f32 xAxisLength = v2_length(xAxis);
  f32 yAxisLength = v2_length(yAxis);
  struct v2 NxAxis = v2_mul(xAxis, yAxisLength / xAxisLength);
  struct v2 NyAxis = v2_mul(yAxis, xAxisLength / yAxisLength);
  f32 NzScale = 0.5f * (xAxisLength + yAxisLength);

  i32 widthMax = (i32)buffer->width - 1;
  i32 heightMax = (i32)buffer->height - 1;

  f32 invWidthMax = 1.0f / (f32)widthMax;
  f32 invHeightMax = 1.0f / (f32)heightMax;

  // TODO(e2dk4r): this will need to be specified seperately
  f32 originZ = 0.0f;
  f32 originY = v2_add(origin, v2_add(v2_mul(xAxis, 0.5f), v2_mul(yAxis, 0.5f))).y;
  f32 fixedCastY = invHeightMax * originY;

  i32 xMin = widthMax;
  i32 xMax = 0;
  i32 yMin = heightMax;
  i32 yMax = 0;

  struct v2 p[4] = {
      origin,
      v2_add(origin, xAxis),
      v2_add(origin, v2_add(xAxis, yAxis)),
      v2_add(origin, yAxis),
  };

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

  for (i32 y = yMin; y <= yMax; y++) {
    u32 *pixel = (u32 *)row;
    for (i32 x = xMin; x <= xMax; x++) {
      struct v2 pixelP = v2i(x, y);
      struct v2 d = v2_sub(pixelP, origin);
      // TODO(e2dk4r): PerpDot()
      // TODO(e2dk4r): Simpler origin
      f32 edge0 = v2_dot(d, v2_neg(v2_perp(xAxis)));
      f32 edge1 = v2_dot(v2_sub(d, xAxis), v2_neg(v2_perp(yAxis)));
      f32 edge2 = v2_dot(v2_sub(d, v2_add(xAxis, yAxis)), v2_perp(xAxis));
      f32 edge3 = v2_dot(v2_sub(d, yAxis), v2_perp(yAxis));

      if (edge0 < 0 && edge1 < 0 && edge2 < 0 && edge3 < 0) {
        struct v2 screenSpaceUV = v2(invWidthMax * (f32)x, fixedCastY);
        f32 zDiff = pixelsToMeters * ((f32)y - originY);

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

          // NOTE(e2dk4r): Rotate normals based on x y axis!
          normal.xy = v2_add(v2_mul(NxAxis, normal.x), v2_mul(NyAxis, normal.y));
          normal.z *= NzScale;
          normal.xyz = v3_normalize(normal.xyz);

          // NOTE(e2dk4r): The eye vector is always assumed to be e = [0, 0, 1]
          // This is just simplified version of reflection -e + 2 eTn n
          struct v3 bounceDirection = v3_mul(normal.xyz, 2.0f * normal.z);
          bounceDirection.z -= 1.0f;

          // TODO(e2dk4r): eventually we need to support two mappings,
          // one for top-down view (which we don't do now) and one for
          // sideways (which is what's happening here).
          bounceDirection.z = -bounceDirection.z;

          f32 z = originZ + zDiff;
          f32 tEnvMap = bounceDirection.y;
          f32 tFarMap = 0.0f;
          struct environment_map *farMap = 0;
          if (tEnvMap < -0.5f) {
            farMap = bottom;
            tFarMap = -1.0f - 2.0f * tEnvMap;
          } else if (tEnvMap > 0.5f) {
            farMap = top;
            tFarMap = 2.0f * (tEnvMap - 0.5f);
          }

          // TODO(e2dk4r): How do we sample from middle map?
          struct v3 lightColor = v3(0.0f, 0.0f, 0.0f);
          if (farMap) {
            f32 distanceFromMapInZ = farMap->z - z;

            struct v3 farMapColor =
                SampleEnvironmentMap(farMap, screenSpaceUV, bounceDirection, normal.w, distanceFromMapInZ);
            lightColor = v3_lerp(lightColor, farMapColor, tFarMap);
          }

          v3_add_ref(&texel.rgb, v3_mul(lightColor, texel.a));
#if 0
          // NOTE(e2dk4r): draws the bounce direction
          texel.rgb = v3_add(v3(0.5f, 0.5f, 0.5f), v3_mul(bounceDirection, 0.5f));
          v3_mul_ref(&texel.rgb, texel.a);
#endif
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

        Pack4x8(pixel, blended);
      }

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
DrawBitmap(struct bitmap *buffer, struct bitmap *bitmap, struct v2 pos, f32 cAlpha)
{
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
      struct v4 texel = Unpack4x8(src);
      texel = sRGB255toLinear1(texel);
      v4_mul_ref(&texel, cAlpha);

      // destination channels
      struct v4 d = Unpack4x8(dst);
      d = sRGB255toLinear1(d);

      /*
       * Math of calculating blended alpha
       * videoId:   bidrZj1YosA
       * timestamp: 01:06:19
       */
      struct v4 blended = v4_add(v4_mul(d, 1.0f - texel.a), texel);

      blended = Linear1tosRGB255(blended);
      Pack4x8(dst, blended);

      dst++;
      src++;
    }

    dstRow += buffer->stride;
    srcRow += bitmap->stride;
  }
}

struct render_entity_basis_p_result {
  u8 valid : 1;
  struct v2 p;
  f32 scale;
};

internal struct render_entity_basis_p_result
GetRenderEntityBasisP(struct render_group *renderGroup, struct render_entity_basis *entityBasis, struct v2 screenCenter)
{
  struct render_entity_basis_p_result result = {};
  f32 metersToPixels = renderGroup->metersToPixels;

  struct v3 entityBasePosition = v3_mul(entityBasis->basis->position, metersToPixels);

  f32 focalLength = 20.0f * metersToPixels;
  f32 cameraDistanceAboveTarget = 20.0f * metersToPixels;
  f32 distanceToPZ = cameraDistanceAboveTarget - entityBasePosition.z;
  f32 nearClipPlane = 0.2f * metersToPixels;

  struct v3 rawXY = v2_to_v3(v2_add(entityBasePosition.xy, entityBasis->offset.xy), 1.0f);

  if (distanceToPZ <= nearClipPlane)
    return result;

  struct v3 projectedXY = v3_mul(v3_mul(rawXY, focalLength), 1.0f / distanceToPZ);
  result.p = v2_add(screenCenter, projectedXY.xy);
  result.scale = projectedXY.z;
  result.valid = 1;

  return result;
}

inline void
DrawRenderGroup(struct render_group *renderGroup, struct bitmap *outputTarget)
{
  f32 metersToPixels = renderGroup->metersToPixels;
  f32 pixelsToMeters = 1.0f / metersToPixels;
  struct v2 screenWidthHeight = v2u(outputTarget->width, outputTarget->height);
  struct v2 screenCenter = v2_mul(screenWidthHeight, 0.5f);

  for (u32 pushBufferIndex = 0; pushBufferIndex < renderGroup->pushBufferSize;) {
    struct render_group_entry *header = renderGroup->pushBufferBase + pushBufferIndex;
    pushBufferIndex += sizeof(*header);
    void *data = (u8 *)header + sizeof(*header);

    if (header->type == RENDER_GROUP_ENTRY_TYPE_CLEAR) {
      struct render_group_entry_clear *entry = data;
      pushBufferIndex += sizeof(*entry);

      DrawRectangle(outputTarget, v2(0.0f, 0.0f), screenWidthHeight, entry->color);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_BITMAP) {
      struct render_group_entry_bitmap *entry = data;
      pushBufferIndex += sizeof(*entry);

      assert(entry->bitmap);

      struct render_entity_basis_p_result basis = GetRenderEntityBasisP(renderGroup, &entry->basis, screenCenter);
      if (!basis.valid || basis.scale <= 0.0f)
        continue;

#if 0
      DrawBitmap(outputTarget, entry->bitmap, basis.p, entry->alpha);
#else
      DrawRectangleSlowly(outputTarget, basis.p, v2_mul(v2u(entry->bitmap->width, 0), basis.scale),
                          v2_mul(v2u(0, entry->bitmap->height), basis.scale), v4(1.0f, 1.0f, 1.0f, entry->alpha),
                          entry->bitmap, 0, 0, 0, 0, pixelsToMeters);
#endif
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_RECTANGLE) {
      struct render_group_entry_rectangle *entry = data;
      pushBufferIndex += sizeof(*entry);

      struct render_entity_basis_p_result basis = GetRenderEntityBasisP(renderGroup, &entry->basis, screenCenter);
      if (!basis.valid || basis.scale <= 0.0f)
        continue;
      struct v2 halfDim = v2_mul(v2_mul(v2_mul(entry->dim, 0.5f), basis.scale), metersToPixels);
      DrawRectangle(outputTarget, v2_sub(basis.p, halfDim), v2_add(basis.p, halfDim), entry->color);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_COORDINATE_SYSTEM) {
      struct render_group_entry_coordinate_system *entry = data;
      pushBufferIndex += sizeof(*entry);

      DrawRectangleSlowly(outputTarget, entry->origin, entry->xAxis, entry->yAxis, entry->color, entry->texture,
                          entry->normalMap, entry->top, entry->middle, entry->bottom, pixelsToMeters);

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
