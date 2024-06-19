#include <handmadehero/analysis.h>
#include <handmadehero/handmadehero.h>
#include <handmadehero/render_group.h>
#include <x86intrin.h>

struct render_group *
RenderGroup(struct memory_arena *arena, u64 pushBufferTotal)
{
  struct render_group *renderGroup = MemoryArenaPush(arena, sizeof(*renderGroup));

  renderGroup->pushBufferSize = 0;
  renderGroup->pushBufferTotal = pushBufferTotal;
  renderGroup->pushBufferBase = MemoryArenaPush(arena, renderGroup->pushBufferTotal);

  renderGroup->alpha = 1.0f;

  // Default transform
  renderGroup->transform.offsetP = v3(0.0f, 0.0f, 0.0f);
  renderGroup->transform.scale = 1.0f;

  return renderGroup;
}

void
RenderGroupPerspective(struct render_group *renderGroup, u32 pixelWidth, u32 pixelHeight)
{
  f32 widthOfMonitor = 0.635f; // 25" in meters
  f32 metersToPixels = (f32)pixelWidth * widthOfMonitor;
  f32 pixelsToMeters = 1.0f / metersToPixels;
  renderGroup->monitorHalfDimInMeters =
      v2((f32)pixelWidth * pixelsToMeters * 0.5f, (f32)pixelHeight * pixelsToMeters * 0.5f);

  struct render_transform *transform = &renderGroup->transform;
  transform->focalLength = 0.6f;
  transform->distanceAboveTarget = 9.0f;
  transform->metersToPixels = metersToPixels;
  transform->screenCenter = v2((f32)pixelWidth * 0.5f, (f32)pixelHeight * 0.5f);
  transform->isOrthographic = 0;
}

void
RenderGroupOrthographic(struct render_group *renderGroup, u32 pixelWidth, u32 pixelHeight, f32 metersToPixels)
{
  f32 pixelsToMeters = 1.0f / metersToPixels;
  renderGroup->monitorHalfDimInMeters =
      v2((f32)pixelWidth * pixelsToMeters * 0.5f, (f32)pixelHeight * pixelsToMeters * 0.5f);

  struct render_transform *transform = &renderGroup->transform;
  transform->focalLength = 1.0f;
  transform->distanceAboveTarget = 1.0f;
  transform->metersToPixels = metersToPixels;
  transform->screenCenter = v2((f32)pixelWidth * 0.5f, (f32)pixelHeight * 0.5f);
  transform->isOrthographic = 1;
}

internal inline struct v2
Unproject(struct render_group *renderGroup, struct v2 projectedXY, f32 atDistanceFromCamera)
{
  struct v2 worldXY = v2_mul(projectedXY, (atDistanceFromCamera / renderGroup->transform.focalLength));
  return worldXY;
}

inline struct rect2
GetCameraRectangleAtDistance(struct render_group *renderGroup, f32 distanceFromCamera)
{
  struct v2 rawXY = Unproject(renderGroup, renderGroup->monitorHalfDimInMeters, distanceFromCamera);

  struct rect2 result = Rect2CenterHalfDim(v2(0.0f, 0.0f), rawXY);

  return result;
}

inline struct rect2
GetCameraRectangleAtTarget(struct render_group *renderGroup)
{
  struct rect2 result = GetCameraRectangleAtDistance(renderGroup, renderGroup->transform.distanceAboveTarget);
  return result;
}

struct render_entity_basis_p_result {
  u8 valid : 1;
  struct v2 p;
  f32 scale;
};

internal struct render_entity_basis_p_result
GetRenderEntityBasisP(struct render_transform *transform, struct v3 originalPosition)
{
  struct render_entity_basis_p_result result = {};

  struct v3 position = v3_add(v2_to_v3(originalPosition.xy, 0.0f), transform->offsetP);

  if (transform->isOrthographic) {
    result.p = v2_add(transform->screenCenter, v2_mul(position.xy, transform->metersToPixels));
    result.scale = transform->metersToPixels;
    result.valid = 1;
  } else {
    f32 offsetZ = 0.0f;

    f32 distanceAboveTarget = transform->distanceAboveTarget;
#if 0
  // TODO(e2dk4r): how do we want to control the debug camera?
  distanceAboveTarget += 30.0f;
#endif

    f32 distanceToPZ = distanceAboveTarget - position.z;
    f32 nearClipPlane = 0.2f;

    struct v3 rawXY = v2_to_v3(position.xy, 1.0f);

    if (distanceToPZ <= nearClipPlane)
      return result;

    struct v3 projectedXY = v3_mul(v3_mul(rawXY, transform->focalLength), 1.0f / distanceToPZ);
    result.scale = projectedXY.z * transform->metersToPixels;
    result.p = v2_add(v2_add(transform->screenCenter, v2_mul(projectedXY.xy, transform->metersToPixels)),
                      v2(0.0f, offsetZ * result.scale));
    result.valid = 1;
  }

  return result;
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
PushBitmapEntry(struct render_group *renderGroup, struct bitmap *bitmap, struct v3 offset, f32 height, struct v4 color)
{
  struct v2 size = v2(height * bitmap->widthOverHeight, height);
  struct v2 alignPixel = v2_hadamard(bitmap->alignPercentage, size);
  struct v3 position = v3_sub(offset, v2_to_v3(alignPixel, 0.0f));

  struct render_entity_basis_p_result basis = GetRenderEntityBasisP(&renderGroup->transform, position);
  if (!basis.valid || basis.scale <= 0.0f)
    return;

  struct render_group_entry_bitmap *entry =
      PushRenderEntry(renderGroup, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_BITMAP);
  entry->bitmap = bitmap;
  entry->size = v2_mul(size, basis.scale);
  entry->position = basis.p;
  entry->color = color;
}

internal inline void
PushRectangleEntry(struct render_group *renderGroup, struct v3 offset, struct v2 dim, struct v4 color)
{
  struct v3 position = v3_sub(offset, v2_to_v3(v2_mul(dim, 0.5f), 0.0f));
  struct render_entity_basis_p_result basis = GetRenderEntityBasisP(&renderGroup->transform, position);
  if (!basis.valid || basis.scale <= 0.0f)
    return;

  struct render_group_entry_rectangle *rect =
      PushRenderEntry(renderGroup, sizeof(*rect), RENDER_GROUP_ENTRY_TYPE_RECTANGLE);
  rect->position = basis.p;
  rect->dim = v2_mul(dim, basis.scale);
  rect->color = color;
}

inline void
CoordinateSystem(struct render_group *renderGroup, struct v2 origin, struct v2 xAxis, struct v2 yAxis, struct v4 color,
                 struct bitmap *texture, struct bitmap *normalMap, struct environment_map *top,
                 struct environment_map *middle, struct environment_map *bottom)
{
#if 0
  struct render_entity_basis_p_result basis = GetRenderEntityBasisP(&renderGroup->transform);
  if (!basis.valid || basis.scale <= 0.0f)
    return;

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
#endif
}

void
Clear(struct render_group *renderGroup, struct v4 color)
{
  PushClearEntry(renderGroup, color);
}

inline void
Bitmap(struct render_group *renderGroup, struct bitmap *bitmap, struct v3 offset, f32 height)
{
  PushBitmapEntry(renderGroup, bitmap, offset, height, v4(1.0f, 1.0f, 1.0f, renderGroup->alpha * 1.0f));
}

inline void
BitmapWithColor(struct render_group *renderGroup, struct bitmap *bitmap, struct v3 offset, f32 height, struct v4 color)
{
  color.a *= renderGroup->alpha;
  PushBitmapEntry(renderGroup, bitmap, offset, height, color);
}

inline void
Rect(struct render_group *renderGroup, struct v3 offset, struct v2 dim, struct v4 color)
{
  PushRectangleEntry(renderGroup, offset, dim, color);
}

inline void
RectOutline(struct render_group *renderGroup, struct v3 offset, struct v2 dim, struct v4 color)
{
#if 1
  f32 thickness = 0.1f;

  // NOTE(e2dk4r): top and bottom
  PushRectangleEntry(renderGroup, v3_sub(offset, v3(0.0f, 0.5f * dim.y, 0.0f)), v2(dim.x, thickness), color);
  PushRectangleEntry(renderGroup, v3_add(offset, v3(0.0f, 0.5f * dim.y, 0.0f)), v2(dim.x, thickness), color);

  // NOTE(e2dk4r): left right
  PushRectangleEntry(renderGroup, v3_sub(offset, v3(0.5f * dim.x, 0.0f, 0.0f)), v2(thickness, dim.y), color);
  PushRectangleEntry(renderGroup, v3_add(offset, v3(0.5f * dim.x, 0.0f, 0.0f)), v2(thickness, dim.y), color);
#endif
}

inline void
DrawRectangle(struct bitmap *buffer, struct v2 min, struct v2 max, const struct v4 color, struct rect2i clipRect,
              b32 even)
{
  assert(min.x < max.x);
  assert(min.y < max.y);

  struct rect2i fillRect = {roundf32toi32(min.x), roundf32toi32(min.y), roundf32toi32(max.x), roundf32toi32(max.y)};
  fillRect = Rect2iIntersect(fillRect, clipRect);
  if (!even == ((fillRect.minY & 1) != 0)) {
    fillRect.minY += 1;
  }

  u8 *row = buffer->memory
            /* x offset */
            + (fillRect.minX * BITMAP_BYTES_PER_PIXEL)
            /* y offset */
            + (fillRect.minY * buffer->stride);

  u32 colorRGBA =
      /* alpha */
      roundf32tou32(color.a * 255.0f) << 24
      /* red */
      | roundf32tou32(color.r * 255.0f) << 16
      /* green */
      | roundf32tou32(color.g * 255.0f) << 8
      /* blue */
      | roundf32tou32(color.b * 255.0f) << 0;

  for (i32 y = fillRect.minY; y < fillRect.maxY; y += 2) {
    u32 *pixel = (u32 *)row;
    for (i32 x = fillRect.minX; x < fillRect.maxX; x++) {
      *pixel = colorRGBA;
      pixel++;
    }
    row += 2 * buffer->stride;
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
  BEGIN_TIMER_BLOCK(DrawRectangleSlowly);

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

#if 0
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
#endif

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

  END_TIMER_BLOCK(DrawRectangleSlowly);
}

#if COMPILER_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
internal inline void
DrawRectangleQuickly(struct bitmap *buffer, struct v2 origin, struct v2 xAxis, struct v2 yAxis, struct v4 color,
                     struct bitmap *texture, f32 pixelsToMeters, struct rect2i clipRect, b32 even)
{
  BEGIN_TIMER_BLOCK(DrawRectangleQuickly);

  f32 InvXAxisLengthSq = 1.0f / v2_length_square(xAxis);
  f32 InvYAxisLengthSq = 1.0f / v2_length_square(yAxis);

  f32 xAxisLength = v2_length(xAxis);
  f32 yAxisLength = v2_length(yAxis);
  struct v2 NxAxis = v2_mul(xAxis, yAxisLength / xAxisLength);
  struct v2 NyAxis = v2_mul(yAxis, xAxisLength / yAxisLength);
  f32 NzScale = 0.5f * (xAxisLength + yAxisLength);

  __m128 half = _mm_set1_ps(0.5f);
  // TODO(e2dk4r): IMPORTANT: REMOVE THIS ONCE WE HAVE REAL ROW LOADING
  // i32 widthMax = (i32)buffer->width - 3;
  // i32 heightMax = (i32)buffer->height - 3;

  // TODO(e2dk4r): this will need to be specified seperately
  f32 originZ = 0.0f;
  f32 originY = v2_add(origin, v2_add(v2_mul(xAxis, 0.5f), v2_mul(yAxis, 0.5f))).y;

  struct v2 p[4] = {
      origin,
      v2_add(origin, xAxis),
      v2_add(origin, v2_add(xAxis, yAxis)),
      v2_add(origin, yAxis),
  };

  struct rect2i fillRect = Rect2iInvertedInfinity();
  for (u32 pIndex = 0; pIndex < ARRAY_COUNT(p); pIndex++) {
    struct v2 testP = p[pIndex];
    i32 floorX = Floor(testP.x);
    i32 ceilX = Ceil(testP.x) + 1;
    i32 floorY = Floor(testP.y);
    i32 ceilY = Ceil(testP.y) + 1;

    if (fillRect.minX > floorX)
      fillRect.minX = floorX;

    if (fillRect.maxX < ceilX)
      fillRect.maxX = ceilX;

    if (fillRect.minY > floorY)
      fillRect.minY = floorY;

    if (fillRect.maxY < ceilY)
      fillRect.maxY = ceilY;
  }

  // struct rect2i clipRect = // {0, 0, widthMax, heightMax};
  //                             {128, 128, 256, 256};
  fillRect = Rect2iIntersect(fillRect, clipRect);
  if (!even == ((fillRect.minY & 1) != 0)) {
    fillRect.minY += 1;
  }

  if (!HasRect2iArea(fillRect)) {
    END_TIMER_BLOCK(DrawRectangleQuickly);
    return;
  }

  __m128i startClipMask = _mm_set1_epi8(-1);
  __m128i endClipMask = _mm_set1_epi8(-1);

  __m128i startClipMasks[] = {
      _mm_slli_si128(startClipMask, 0 * 4),
      _mm_slli_si128(startClipMask, 1 * 4),
      _mm_slli_si128(startClipMask, 2 * 4),
      _mm_slli_si128(startClipMask, 3 * 4),
  };

  __m128i endClipMasks[] = {
      _mm_srli_si128(endClipMask, 0 * 4),
      _mm_srli_si128(endClipMask, 3 * 4),
      _mm_srli_si128(endClipMask, 2 * 4),
      _mm_srli_si128(endClipMask, 1 * 4),
  };

  if (fillRect.minX & 3) {
    startClipMask = startClipMasks[fillRect.minX & 3];
    fillRect.minX = fillRect.minX & ~3;
  }

  if (fillRect.maxX & 3) {
    endClipMask = endClipMasks[fillRect.maxX & 3];
    fillRect.maxX = (fillRect.maxX & ~3) + 4;
  }

  u8 *row = buffer->memory + fillRect.minY * buffer->stride + fillRect.minX * BITMAP_BYTES_PER_PIXEL;
  i32 rowAdvance = buffer->stride * 2;

  // pre-multiplied alpha
  v3_mul_ref(&color.rgb, color.a);

  // pre-multiplied axis
  struct v2 nxAxis = v2_mul(xAxis, InvXAxisLengthSq);
  struct v2 nyAxis = v2_mul(yAxis, InvYAxisLengthSq);

  f32 inv255 = 1.0f / 255.0f;

  BEGIN_TIMER_BLOCK(ProcessPixel);
  for (i32 y = fillRect.minY; y < fillRect.maxY; y += 2) {
    u32 *pixel = (u32 *)row;

    __m128 pixelPx = _mm_setr_ps((f32)(fillRect.minX + 0) - origin.x, (f32)(fillRect.minX + 1) - origin.x,
                                 (f32)(fillRect.minX + 2) - origin.x, (f32)(fillRect.minX + 3) - origin.x);
    __m128 pixelPy = _mm_set1_ps((f32)y - origin.y);

    __m128i clipMask = startClipMask;

    for (i32 xi = fillRect.minX; xi < fillRect.maxX; xi += 4) {
      BEGIN_ANALYSIS("ProcessPixel");

      __m128 u = pixelPx * nxAxis.x + pixelPy * nxAxis.y;
      __m128 v = pixelPx * nyAxis.x + pixelPy * nyAxis.y;

#define mmClamp01(a) _mm_min_ps(_mm_max_ps(a, _mm_set1_ps(0.0f)), _mm_set1_ps(1.0f))
      u = mmClamp01(u);
      v = mmClamp01(v);

      __m128i writeMask = (u >= 0.0f) & (u < 1.0f) & (v >= 0.0f) & (v < 1.0f);
      // if (!_mm_movemask_epi8(writeMask))
      //   continue;
      writeMask &= clipMask;

      // Bias texture coordinates to start on the boundry
      // between the 0,0 and 1,1 pixels.
      __m128 bias = half;
      __m128 tX = (u * (f32)(texture->width - 2)) + bias;
      __m128 tY = (v * (f32)(texture->height - 2)) + bias;

      __m128i texelX = _mm_cvttps_epi32(tX);
      __m128i texelY = _mm_cvttps_epi32(tY);

      __m128 fX = tX - _mm_cvtepi32_ps(texelX);
      __m128 fY = tY - _mm_cvtepi32_ps(texelY);

      __m128i originalDest = _mm_load_si128((__m128i *)pixel);

      __m128i sampleA;
      __m128i sampleB;
      __m128i sampleC;
      __m128i sampleD;

#define mi(a, i) *((i32 *)&a + i)

      for (i32 i = 0; i < 4; i++) {
        i32 fetchX = mi(texelX, i);
        i32 fetchY = mi(texelY, i);

        // BilinearSample
        u8 *texelPtr = ((u8 *)texture->memory + fetchY * texture->stride + fetchX * BITMAP_BYTES_PER_PIXEL);
        mi(sampleA, i) = *(i32 *)(texelPtr);
        mi(sampleB, i) = *(i32 *)(texelPtr + BITMAP_BYTES_PER_PIXEL);
        mi(sampleC, i) = *(i32 *)(texelPtr + texture->stride);
        mi(sampleD, i) = *(i32 *)(texelPtr + texture->stride + BITMAP_BYTES_PER_PIXEL);
      }

      // sRGBBilinearBlend - Unpack4x8
      // texelA
      __m128 texelAr = _mm_cvtepi32_ps(sampleA >> 0x10 & _mm_set1_epi32(0xff));
      __m128 texelAg = _mm_cvtepi32_ps(sampleA >> 0x08 & _mm_set1_epi32(0xff));
      __m128 texelAb = _mm_cvtepi32_ps(sampleA >> 0x00 & _mm_set1_epi32(0xff));
      __m128 texelAa = _mm_cvtepi32_ps(sampleA >> 0x18 & _mm_set1_epi32(0xff));

      // texelB
      __m128 texelBr = _mm_cvtepi32_ps(sampleB >> 0x10 & _mm_set1_epi32(0xff));
      __m128 texelBg = _mm_cvtepi32_ps(sampleB >> 0x08 & _mm_set1_epi32(0xff));
      __m128 texelBb = _mm_cvtepi32_ps(sampleB >> 0x00 & _mm_set1_epi32(0xff));
      __m128 texelBa = _mm_cvtepi32_ps(sampleB >> 0x18 & _mm_set1_epi32(0xff));

      // texelC
      __m128 texelCr = _mm_cvtepi32_ps(sampleC >> 0x10 & _mm_set1_epi32(0xff));
      __m128 texelCg = _mm_cvtepi32_ps(sampleC >> 0x08 & _mm_set1_epi32(0xff));
      __m128 texelCb = _mm_cvtepi32_ps(sampleC >> 0x00 & _mm_set1_epi32(0xff));
      __m128 texelCa = _mm_cvtepi32_ps(sampleC >> 0x18 & _mm_set1_epi32(0xff));

      // texelD
      __m128 texelDr = _mm_cvtepi32_ps(sampleD >> 0x10 & _mm_set1_epi32(0xff));
      __m128 texelDg = _mm_cvtepi32_ps(sampleD >> 0x08 & _mm_set1_epi32(0xff));
      __m128 texelDb = _mm_cvtepi32_ps(sampleD >> 0x00 & _mm_set1_epi32(0xff));
      __m128 texelDa = _mm_cvtepi32_ps(sampleD >> 0x18 & _mm_set1_epi32(0xff));

      // destination channels
      __m128 destr = _mm_cvtepi32_ps(originalDest >> 0x10 & _mm_set1_epi32(0xff));
      __m128 destg = _mm_cvtepi32_ps(originalDest >> 0x08 & _mm_set1_epi32(0xff));
      __m128 destb = _mm_cvtepi32_ps(originalDest >> 0x00 & _mm_set1_epi32(0xff));
      __m128 desta = _mm_cvtepi32_ps(originalDest >> 0x18 & _mm_set1_epi32(0xff));

#define mmSquare(a) (a * a)
      // sRGBBilinearBlend - sRGB255toLinear1()
      texelAr = mmSquare(texelAr);
      texelAg = mmSquare(texelAg);
      texelAb = mmSquare(texelAb);

      texelBr = mmSquare(texelBr);
      texelBg = mmSquare(texelBg);
      texelBb = mmSquare(texelBb);

      texelCr = mmSquare(texelCr);
      texelCg = mmSquare(texelCg);
      texelCb = mmSquare(texelCb);

      texelDr = mmSquare(texelDr);
      texelDg = mmSquare(texelDg);
      texelDb = mmSquare(texelDb);

      // sRGBBilinearBlend - v4_lerp()
      __m128 invfX = 1.0f - fX;
      __m128 invfY = 1.0f - fY;

      __m128 l0 = invfX * invfY;
      __m128 l1 = fX * invfY;
      __m128 l2 = invfX * fY;
      __m128 l3 = fX * fY;

      __m128 texelr = texelAr * l0 + texelBr * l1 + texelCr * l2 + texelDr * l3;
      __m128 texelg = texelAg * l0 + texelBg * l1 + texelCg * l2 + texelDg * l3;
      __m128 texelb = texelAb * l0 + texelBb * l1 + texelCb * l2 + texelDb * l3;
      __m128 texela = texelAa * l0 + texelBa * l1 + texelCa * l2 + texelDa * l3;

      // v4_hadamard(texel, color)
      texelr = texelr * color.r;
      texelg = texelg * color.g;
      texelb = texelb * color.b;
      texela = texela * color.a;

#define mmClamp0(a, max) _mm_min_ps(_mm_max_ps(a, _mm_set1_ps(0.0f)), _mm_set1_ps(max))
      texelr = mmClamp0(texelr, Square(255.0f));
      texelg = mmClamp0(texelg, Square(255.0f));
      texelb = mmClamp0(texelb, Square(255.0f));

      // NOTE(e2dk4r): Go from sRGB to "linear" brightness space
      destr = mmSquare(destr);
      destg = mmSquare(destg);
      destb = mmSquare(destb);
      // desta = desta;

      // blend alpha
      __m128 invTexela = 1.0f - inv255 * texela;
      __m128 blendedr = destr * invTexela + texelr;
      __m128 blendedg = destg * invTexela + texelg;
      __m128 blendedb = destb * invTexela + texelb;
      __m128 blendeda = desta * invTexela + texela;

      // NOTE(e2dk4r): Go from "linear" brightness space to sRGB
      blendedr *= _mm_rsqrt_ps(blendedr);
      blendedg *= _mm_rsqrt_ps(blendedg);
      blendedb *= _mm_rsqrt_ps(blendedb);
      // blendeda = blendeda;

      __m128i intr = _mm_cvtps_epi32(blendedr);
      __m128i intg = _mm_cvtps_epi32(blendedg);
      __m128i intb = _mm_cvtps_epi32(blendedb);
      __m128i inta = _mm_cvtps_epi32(blendeda);

      __m128i out = intr << 0x10 | intg << 0x08 | intb << 0x00 | inta << 0x18;

      __m128i maskedOut = (out & writeMask) | (originalDest & ~writeMask);
      _mm_store_si128((__m128i *)pixel, maskedOut);

      pixel += 4;
      pixelPx += 4;

      clipMask = _mm_set1_epi8(-1);
      if (xi + 8 >= fillRect.maxX) {
        clipMask = endClipMask;
      }

      END_ANALYSIS();
    }

    row += rowAdvance;
  }

  END_TIMER_BLOCK_COUNTED(ProcessPixel, Rect2iArea(fillRect) / 2);

  END_TIMER_BLOCK(DrawRectangleQuickly);
}
#if COMPILER_GCC
#pragma GCC diagnostic pop
#endif

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

struct tile_render_work {
  struct render_group *renderGroup;
  struct bitmap *outputTarget;
  struct rect2i clipRect;
};

internal void
DoTiledRenderWork(struct platform_work_queue *queue, void *data)
{
  struct tile_render_work *work = data;

  DrawRenderGroup(work->renderGroup, work->outputTarget, work->clipRect, 0);
  DrawRenderGroup(work->renderGroup, work->outputTarget, work->clipRect, 1);
}

inline void
TiledDrawRenderGroup(struct platform_work_queue *renderQueue, struct render_group *renderGroup,
                     struct bitmap *outputTarget)
{
  /* TODO(e2dk4r):
   *
   *   - make sure that tiles are cache-aligned
   *   - get hyperthreads synced so they do interleaved lines?
   *   - how big should the tiles be for performance?
   *   - ballpark the memory bandwidth for DrawRectangleQuickly
   *   - Re-test some of our instruction choices
   *
   */
  i32 tileCountX = 4;
  i32 tileCountY = 4;

  struct tile_render_work workArray[tileCountX * tileCountY];
  u32 workCount = 0;

  assert(((uptr)outputTarget->memory & (4 * BITMAP_BYTES_PER_PIXEL - 1)) == 0 &&
         "must be aligned to 4 pixels (4x4 bytes)");
  i32 tileWidth = (i32)outputTarget->width / tileCountX;
  i32 tileHeight = (i32)outputTarget->height / tileCountY;

  tileWidth = (tileWidth + 3) / 4 * 4;

  for (i32 tileY = 0; tileY < tileCountY; tileY++) {
    for (i32 tileX = 0; tileX < tileCountX; tileX++) {
      struct rect2i clipRect;
      clipRect.minX = tileX * tileWidth;
      clipRect.maxX = clipRect.minX + tileWidth;
      clipRect.minY = tileY * tileHeight;
      clipRect.maxY = clipRect.minY + tileHeight;

      if (tileX == tileCountX - 1)
        clipRect.maxX = (i32)outputTarget->width;
      if (tileY == tileCountY - 1)
        clipRect.maxY = (i32)outputTarget->height;

      u32 workIndex = workCount;
      struct tile_render_work *work = workArray + workIndex;
      work->renderGroup = renderGroup;
      work->outputTarget = outputTarget;
      work->clipRect = clipRect;
      workCount++;

#if 1
      /* rendering multi-threaded */
      PlatformWorkQueueAddEntry(renderQueue, DoTiledRenderWork, work);
#else
      /* rendering single-threaded */
      DoTiledRenderWork(renderQueue, work);
#endif
    }
  }

  PlatformWorkQueueCompleteAllWork(renderQueue);
}

inline void
DrawRenderGroup(struct render_group *renderGroup, struct bitmap *outputTarget, struct rect2i clipRect, b32 even)
{
  BEGIN_TIMER_BLOCK(DrawRenderGroup);

  f32 pixelsToMeters = 1.0f / renderGroup->transform.metersToPixels;

  for (u32 pushBufferIndex = 0; pushBufferIndex < renderGroup->pushBufferSize;) {
    struct render_group_entry *header = renderGroup->pushBufferBase + pushBufferIndex;
    pushBufferIndex += sizeof(*header);
    void *data = (u8 *)header + sizeof(*header);

    if (header->type == RENDER_GROUP_ENTRY_TYPE_CLEAR) {
      struct render_group_entry_clear *entry = data;
      pushBufferIndex += sizeof(*entry);

      struct v2 screenDim = v2u(outputTarget->width, outputTarget->height);
      DrawRectangle(outputTarget, v2(0.0f, 0.0f), screenDim, entry->color, clipRect, even);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_BITMAP) {
      struct render_group_entry_bitmap *entry = data;
      pushBufferIndex += sizeof(*entry);

      assert(entry->bitmap);

#if 0
      DrawBitmap(outputTarget, entry->bitmap, basis.p, entry->alpha);
#else
      struct v2 xAxis = v2(1.0f, 0.0f);
      struct v2 yAxis = v2_perp(xAxis);
      DrawRectangleQuickly(outputTarget, entry->position, v2_mul(xAxis, entry->size.x), v2_mul(yAxis, entry->size.y),
                           entry->color, entry->bitmap, pixelsToMeters, clipRect, even);
#endif
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_RECTANGLE) {
      struct render_group_entry_rectangle *entry = data;
      pushBufferIndex += sizeof(*entry);

      DrawRectangle(outputTarget, entry->position, v2_add(entry->position, entry->dim), entry->color, clipRect, even);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_COORDINATE_SYSTEM) {
      struct render_group_entry_coordinate_system *entry = data;
      pushBufferIndex += sizeof(*entry);

#if 0
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
#endif
    }

    // typelessEntry->type is invalid
    else {
      assert(0 && "this renderer does not know how to handle render group entry type");
    }
  }

  END_TIMER_BLOCK(DrawRenderGroup);
}
