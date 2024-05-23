#include <handmadehero/handmadehero.h>
#include <handmadehero/render_group.h>
#include <x86intrin.h>

struct render_group *
RenderGroup(struct memory_arena *arena, u64 pushBufferTotal, u32 resolutionPixelsX, u32 resolutionPixelsY)
{
  struct render_group *renderGroup = MemoryArenaPush(arena, sizeof(*renderGroup));

  renderGroup->defaultBasis = MemoryArenaPush(arena, sizeof(*renderGroup->defaultBasis));
  renderGroup->defaultBasis->position = v3(0.0f, 0.0f, 0.0f);

  renderGroup->alpha = 1.0f;

  renderGroup->pushBufferSize = 0;
  renderGroup->pushBufferTotal = pushBufferTotal;
  renderGroup->pushBufferBase = MemoryArenaPush(arena, renderGroup->pushBufferTotal);

  renderGroup->gameCamera.focalLength = 0.6f;
  renderGroup->gameCamera.cameraDistanceAboveTarget = 9.0f;

  // NOTE(e2dk4r): Horizontal measurement of monitor in meters
  f32 widthOfMonitor = 0.625f; // 25" in meters
  renderGroup->metersToPixels = (f32)resolutionPixelsX * widthOfMonitor;

  f32 pixelsToMeters = 1.0f / renderGroup->metersToPixels;
  renderGroup->monitorHalfDimInMeters =
      v2((f32)resolutionPixelsX * pixelsToMeters * 0.5f, (f32)resolutionPixelsY * pixelsToMeters * 0.5f);

  renderGroup->renderCamera = renderGroup->gameCamera;
  // renderGroup->renderCamera.cameraDistanceAboveTarget = 50.0f;

  return renderGroup;
}

internal inline struct v2
Unproject(struct render_group *renderGroup, f32 atDistanceFromCamera)
{
  struct v2 worldXY =
      v2_mul(renderGroup->monitorHalfDimInMeters, (atDistanceFromCamera / renderGroup->gameCamera.focalLength));
  return worldXY;
}

inline struct rect2
GetCameraRectangleAtDistance(struct render_group *renderGroup, f32 distanceFromCamera)
{
  struct v2 rawXY = Unproject(renderGroup, distanceFromCamera);

  struct rect2 result = Rect2CenterHalfDim(v2(0.0f, 0.0f), rawXY);

  return result;
}

inline struct rect2
GetCameraRectangleAtTarget(struct render_group *renderGroup)
{
  struct rect2 result = GetCameraRectangleAtDistance(renderGroup, renderGroup->gameCamera.cameraDistanceAboveTarget);
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
PushBitmapEntry(struct render_group *renderGroup, struct bitmap *bitmap, struct v3 offset, f32 height, f32 alpha)
{
  struct render_group_entry_bitmap *entry =
      PushRenderEntry(renderGroup, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_BITMAP);
  entry->bitmap = bitmap;
  entry->basis.basis = renderGroup->defaultBasis;

  entry->basis.offset = offset;
  entry->size = v2(height * bitmap->widthOverHeight, height);

  struct v2 alignPixel = v2_hadamard(bitmap->alignPercentage, entry->size);
  v2_sub_ref(&entry->basis.offset.xy, alignPixel);

  entry->alpha = alpha;
}

internal inline void
PushRectangleEntry(struct render_group *renderGroup, struct v3 offset, struct v2 dim, struct v4 color)
{
  struct render_group_entry_rectangle *entry =
      PushRenderEntry(renderGroup, sizeof(*entry), RENDER_GROUP_ENTRY_TYPE_RECTANGLE);
  entry->basis.basis = renderGroup->defaultBasis;
  entry->basis.offset = v3_sub(offset, v2_to_v3(v2_mul(dim, 0.5f), 0.0f));
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
Bitmap(struct render_group *renderGroup, struct bitmap *bitmap, struct v3 offset, f32 height)
{
  PushBitmapEntry(renderGroup, bitmap, offset, height, renderGroup->alpha * 1.0f);
}

inline void
BitmapWithAlpha(struct render_group *renderGroup, struct bitmap *bitmap, struct v3 offset, f32 height, f32 alpha)
{
  PushBitmapEntry(renderGroup, bitmap, offset, height, renderGroup->alpha * alpha);
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

internal inline void
DrawRectangleHopefullyQuickly(struct bitmap *buffer, struct v2 origin, struct v2 xAxis, struct v2 yAxis,
                              struct v4 color, struct bitmap *texture, f32 pixelsToMeters)
{
  BEGIN_TIMER_BLOCK(DrawRectangleHopefullyQuickly);

  f32 InvXAxisLengthSq = 1.0f / v2_length_square(xAxis);
  f32 InvYAxisLengthSq = 1.0f / v2_length_square(yAxis);

  f32 xAxisLength = v2_length(xAxis);
  f32 yAxisLength = v2_length(yAxis);
  struct v2 NxAxis = v2_mul(xAxis, yAxisLength / xAxisLength);
  struct v2 NyAxis = v2_mul(yAxis, xAxisLength / yAxisLength);
  f32 NzScale = 0.5f * (xAxisLength + yAxisLength);

  // TODO(e2dk4r): IMPORTANT: REMOVE THIS ONCE WE HAVE REAL ROW LOADING
  i32 widthMax = ((i32)buffer->width - 1) - 3;
  i32 heightMax = ((i32)buffer->height - 1) - 3;

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

  // pre-multiplied axis
  struct v2 nxAxis = v2_mul(xAxis, InvXAxisLengthSq);
  struct v2 nyAxis = v2_mul(yAxis, InvYAxisLengthSq);

  f32 inv255 = 1.0f / 255.0f;

  BEGIN_TIMER_BLOCK(ProcessPixel);
  for (i32 y = yMin; y <= yMax; y++) {
    u32 *pixel = (u32 *)row;
    for (i32 xi = xMin; xi <= xMax; xi += 4) {

      __m128 texelAr;
      __m128 texelAg;
      __m128 texelAb;
      __m128 texelAa;

      __m128 texelBr;
      __m128 texelBg;
      __m128 texelBb;
      __m128 texelBa;

      __m128 texelCr;
      __m128 texelCg;
      __m128 texelCb;
      __m128 texelCa;

      __m128 texelDr;
      __m128 texelDg;
      __m128 texelDb;
      __m128 texelDa;

      __m128 destr;
      __m128 destg;
      __m128 destb;
      __m128 desta;

      __m128 blendedr;
      __m128 blendedg;
      __m128 blendedb;
      __m128 blendeda;

      __m128 pixelPx = _mm_set_ps((f32)(xi + 3), (f32)(xi + 2), (f32)(xi + 1), (f32)(xi + 0));
      __m128 pixelPy = _mm_set1_ps((f32)y);

      __m128 dx = pixelPx - origin.x;
      __m128 dy = pixelPy - origin.y;

      __m128 u = dx * nxAxis.x + dy * nxAxis.y;
      __m128 v = dx * nyAxis.x + dy * nyAxis.y;
#define mmClamp01(a) _mm_min_ps(_mm_max_ps(a, _mm_set1_ps(0.0f)), _mm_set1_ps(1.0f))
      u = mmClamp01(u);
      v = mmClamp01(v);

      // TODO(e2dk4r): Formalize texture boundaries!
      __m128 tX = u * (f32)(texture->width - 2);
      __m128 tY = v * (f32)(texture->height - 2);

      __m128i texelX = _mm_cvttps_epi32(tX);
      __m128i texelY = _mm_cvttps_epi32(tY);

      __m128 fX = tX - _mm_cvtepi32_ps(texelX);
      __m128 fY = tY - _mm_cvtepi32_ps(texelY);

      __m128i originalDest = _mm_loadu_si128((__m128i *)pixel);
      __m128i writeMask =
          _mm_castps_si128(_mm_and_ps(_mm_and_ps(u >= 0.0f, u < 1.0f), _mm_and_ps(v >= 0.0f, v < 1.0f)));

      u32 sampleA[4];
      u32 sampleB[4];
      u32 sampleC[4];
      u32 sampleD[4];

      for (i32 i = 0; i < 4; i++) {
        i32 fetchX = *((i32 *)&texelX + i);
        i32 fetchY = *((i32 *)&texelY + i);
        assert(fetchX >= 0 && fetchX < (i32)texture->width);
        assert(fetchY >= 0 && fetchY < (i32)texture->height);

        // BilinearSample
        u8 *texelPtr = ((u8 *)texture->memory + fetchY * texture->stride + fetchX * BITMAP_BYTES_PER_PIXEL);
        sampleA[i] = *(u32 *)texelPtr;
        sampleB[i] = *(u32 *)(texelPtr + BITMAP_BYTES_PER_PIXEL);
        sampleC[i] = *(u32 *)(texelPtr + texture->stride);
        sampleD[i] = *(u32 *)(texelPtr + texture->stride + BITMAP_BYTES_PER_PIXEL);
      }

      // sRGBBilinearBlend - Unpack4x8
      // texelA
      texelAr[0] = (f32)((*(sampleA + 0) >> 0x10) & 0xff);
      texelAg[0] = (f32)((*(sampleA + 0) >> 0x08) & 0xff);
      texelAb[0] = (f32)((*(sampleA + 0) >> 0x00) & 0xff);
      texelAa[0] = (f32)((*(sampleA + 0) >> 0x18) & 0xff);

      texelAr[1] = (f32)((*(sampleA + 1) >> 0x10) & 0xff);
      texelAg[1] = (f32)((*(sampleA + 1) >> 0x08) & 0xff);
      texelAb[1] = (f32)((*(sampleA + 1) >> 0x00) & 0xff);
      texelAa[1] = (f32)((*(sampleA + 1) >> 0x18) & 0xff);

      texelAr[2] = (f32)((*(sampleA + 2) >> 0x10) & 0xff);
      texelAg[2] = (f32)((*(sampleA + 2) >> 0x08) & 0xff);
      texelAb[2] = (f32)((*(sampleA + 2) >> 0x00) & 0xff);
      texelAa[2] = (f32)((*(sampleA + 2) >> 0x18) & 0xff);

      texelAr[3] = (f32)((*(sampleA + 3) >> 0x10) & 0xff);
      texelAg[3] = (f32)((*(sampleA + 3) >> 0x08) & 0xff);
      texelAb[3] = (f32)((*(sampleA + 3) >> 0x00) & 0xff);
      texelAa[3] = (f32)((*(sampleA + 3) >> 0x18) & 0xff);

      // texelB
      texelBr[0] = (f32)((*(sampleB + 0) >> 0x10) & 0xff);
      texelBg[0] = (f32)((*(sampleB + 0) >> 0x08) & 0xff);
      texelBb[0] = (f32)((*(sampleB + 0) >> 0x00) & 0xff);
      texelBa[0] = (f32)((*(sampleB + 0) >> 0x18) & 0xff);

      texelBr[1] = (f32)((*(sampleB + 1) >> 0x10) & 0xff);
      texelBg[1] = (f32)((*(sampleB + 1) >> 0x08) & 0xff);
      texelBb[1] = (f32)((*(sampleB + 1) >> 0x00) & 0xff);
      texelBa[1] = (f32)((*(sampleB + 1) >> 0x18) & 0xff);

      texelBr[2] = (f32)((*(sampleB + 2) >> 0x10) & 0xff);
      texelBg[2] = (f32)((*(sampleB + 2) >> 0x08) & 0xff);
      texelBb[2] = (f32)((*(sampleB + 2) >> 0x00) & 0xff);
      texelBa[2] = (f32)((*(sampleB + 2) >> 0x18) & 0xff);

      texelBr[3] = (f32)((*(sampleB + 3) >> 0x10) & 0xff);
      texelBg[3] = (f32)((*(sampleB + 3) >> 0x08) & 0xff);
      texelBb[3] = (f32)((*(sampleB + 3) >> 0x00) & 0xff);
      texelBa[3] = (f32)((*(sampleB + 3) >> 0x18) & 0xff);

      // texelC
      texelCr[0] = (f32)((*(sampleC + 0) >> 0x10) & 0xff);
      texelCg[0] = (f32)((*(sampleC + 0) >> 0x08) & 0xff);
      texelCb[0] = (f32)((*(sampleC + 0) >> 0x00) & 0xff);
      texelCa[0] = (f32)((*(sampleC + 0) >> 0x18) & 0xff);

      texelCr[1] = (f32)((*(sampleC + 1) >> 0x10) & 0xff);
      texelCg[1] = (f32)((*(sampleC + 1) >> 0x08) & 0xff);
      texelCb[1] = (f32)((*(sampleC + 1) >> 0x00) & 0xff);
      texelCa[1] = (f32)((*(sampleC + 1) >> 0x18) & 0xff);

      texelCr[2] = (f32)((*(sampleC + 2) >> 0x10) & 0xff);
      texelCg[2] = (f32)((*(sampleC + 2) >> 0x08) & 0xff);
      texelCb[2] = (f32)((*(sampleC + 2) >> 0x00) & 0xff);
      texelCa[2] = (f32)((*(sampleC + 2) >> 0x18) & 0xff);

      texelCr[3] = (f32)((*(sampleC + 3) >> 0x10) & 0xff);
      texelCg[3] = (f32)((*(sampleC + 3) >> 0x08) & 0xff);
      texelCb[3] = (f32)((*(sampleC + 3) >> 0x00) & 0xff);
      texelCa[3] = (f32)((*(sampleC + 3) >> 0x18) & 0xff);

      // texelD
      texelDr[0] = (f32)((*(sampleD + 0) >> 0x10) & 0xff);
      texelDg[0] = (f32)((*(sampleD + 0) >> 0x08) & 0xff);
      texelDb[0] = (f32)((*(sampleD + 0) >> 0x00) & 0xff);
      texelDa[0] = (f32)((*(sampleD + 0) >> 0x18) & 0xff);

      texelDr[1] = (f32)((*(sampleD + 1) >> 0x10) & 0xff);
      texelDg[1] = (f32)((*(sampleD + 1) >> 0x08) & 0xff);
      texelDb[1] = (f32)((*(sampleD + 1) >> 0x00) & 0xff);
      texelDa[1] = (f32)((*(sampleD + 1) >> 0x18) & 0xff);

      texelDr[2] = (f32)((*(sampleD + 2) >> 0x10) & 0xff);
      texelDg[2] = (f32)((*(sampleD + 2) >> 0x08) & 0xff);
      texelDb[2] = (f32)((*(sampleD + 2) >> 0x00) & 0xff);
      texelDa[2] = (f32)((*(sampleD + 2) >> 0x18) & 0xff);

      texelDr[3] = (f32)((*(sampleD + 3) >> 0x10) & 0xff);
      texelDg[3] = (f32)((*(sampleD + 3) >> 0x08) & 0xff);
      texelDb[3] = (f32)((*(sampleD + 3) >> 0x00) & 0xff);
      texelDa[3] = (f32)((*(sampleD + 3) >> 0x18) & 0xff);

      // destination channels
      destr[0] = (f32)((*(pixel + 0) >> 0x10) & 0xff);
      destg[0] = (f32)((*(pixel + 0) >> 0x08) & 0xff);
      destb[0] = (f32)((*(pixel + 0) >> 0x00) & 0xff);
      desta[0] = (f32)((*(pixel + 0) >> 0x18) & 0xff);

      destr[1] = (f32)((*(pixel + 1) >> 0x10) & 0xff);
      destg[1] = (f32)((*(pixel + 1) >> 0x08) & 0xff);
      destb[1] = (f32)((*(pixel + 1) >> 0x00) & 0xff);
      desta[1] = (f32)((*(pixel + 1) >> 0x18) & 0xff);

      destr[2] = (f32)((*(pixel + 2) >> 0x10) & 0xff);
      destg[2] = (f32)((*(pixel + 2) >> 0x08) & 0xff);
      destb[2] = (f32)((*(pixel + 2) >> 0x00) & 0xff);
      desta[2] = (f32)((*(pixel + 2) >> 0x18) & 0xff);

      destr[3] = (f32)((*(pixel + 3) >> 0x10) & 0xff);
      destg[3] = (f32)((*(pixel + 3) >> 0x08) & 0xff);
      destb[3] = (f32)((*(pixel + 3) >> 0x00) & 0xff);
      desta[3] = (f32)((*(pixel + 3) >> 0x18) & 0xff);

#define mmSquare(a) (a * a)
      // sRGBBilinearBlend - sRGB255toLinear1()
      texelAr = mmSquare(inv255 * texelAr);
      texelAg = mmSquare(inv255 * texelAg);
      texelAb = mmSquare(inv255 * texelAb);
      texelAa = inv255 * texelAa;

      texelBr = mmSquare(inv255 * texelBr);
      texelBg = mmSquare(inv255 * texelBg);
      texelBb = mmSquare(inv255 * texelBb);
      texelBa = inv255 * texelBa;

      texelCr = mmSquare(inv255 * texelCr);
      texelCg = mmSquare(inv255 * texelCg);
      texelCb = mmSquare(inv255 * texelCb);
      texelCa = inv255 * texelCa;

      texelDr = mmSquare(inv255 * texelDr);
      texelDg = mmSquare(inv255 * texelDg);
      texelDb = mmSquare(inv255 * texelDb);
      texelDa = inv255 * texelDa;

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

      texelr = mmClamp01(texelr);
      texelg = mmClamp01(texelg);
      texelb = mmClamp01(texelb);

      // NOTE(e2dk4r): Go from sRGB to "linear" brightness space
      destr = mmSquare(inv255 * destr);
      destg = mmSquare(inv255 * destg);
      destb = mmSquare(inv255 * destb);
      desta = inv255 * desta;

      // blend alpha
      __m128 invTexela = 1.0f - texela;
      blendedr = destr * invTexela + texelr;
      blendedg = destg * invTexela + texelg;
      blendedb = destb * invTexela + texelb;
      blendeda = desta * invTexela + texela;

      // NOTE(e2dk4r): Go from "linear" brightness space to sRGB
      blendedr = 255.0f * _mm_sqrt_ps(blendedr);
      blendedg = 255.0f * _mm_sqrt_ps(blendedg);
      blendedb = 255.0f * _mm_sqrt_ps(blendedb);
      blendeda = 255.0f * blendeda;

      __m128i intr = _mm_cvtps_epi32(blendedr);
      __m128i intg = _mm_cvtps_epi32(blendedg);
      __m128i intb = _mm_cvtps_epi32(blendedb);
      __m128i inta = _mm_cvtps_epi32(blendeda);

      __m128i out = intr << 0x10 | intg << 0x08 | intb << 0x00 | inta << 0x18;

      __m128i maskedOut = (out & writeMask) | (originalDest & ~writeMask);
      _mm_storeu_si128((__m128i *)pixel, maskedOut);

      pixel += 4;
    }

    row += buffer->stride;
  }
  END_TIMER_BLOCK_COUNTED(ProcessPixel, (u64)((xMax - xMin + 1) * (yMax - yMin + 1)));

  END_TIMER_BLOCK(DrawRectangleHopefullyQuickly);
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
GetRenderEntityBasisP(struct render_entity_basis *entityBasis, struct render_group *renderGroup, struct v2 screenDim)
{
  struct render_entity_basis_p_result result = {};

  struct v2 screenCenter = v2_mul(screenDim, 0.5f);
  struct v3 entityBasePosition = entityBasis->basis->position;

  f32 distanceToPZ = renderGroup->renderCamera.cameraDistanceAboveTarget - entityBasePosition.z;
  f32 nearClipPlane = 0.2f;

  struct v3 rawXY = v2_to_v3(v2_add(entityBasePosition.xy, entityBasis->offset.xy), 1.0f);

  if (distanceToPZ <= nearClipPlane)
    return result;

  struct v3 projectedXY = v3_mul(v3_mul(rawXY, renderGroup->renderCamera.focalLength), 1.0f / distanceToPZ);
  result.p = v2_add(screenCenter, v2_mul(projectedXY.xy, renderGroup->metersToPixels));
  result.scale = projectedXY.z * renderGroup->metersToPixels;
  result.valid = 1;

  return result;
}

inline void
DrawRenderGroup(struct render_group *renderGroup, struct bitmap *outputTarget)
{
  BEGIN_TIMER_BLOCK(DrawRenderGroup);

  struct v2 screenDim = v2u(outputTarget->width, outputTarget->height);

  f32 pixelsToMeters = 1.0f / renderGroup->metersToPixels;

  for (u32 pushBufferIndex = 0; pushBufferIndex < renderGroup->pushBufferSize;) {
    struct render_group_entry *header = renderGroup->pushBufferBase + pushBufferIndex;
    pushBufferIndex += sizeof(*header);
    void *data = (u8 *)header + sizeof(*header);

    if (header->type == RENDER_GROUP_ENTRY_TYPE_CLEAR) {
      struct render_group_entry_clear *entry = data;
      pushBufferIndex += sizeof(*entry);

      DrawRectangle(outputTarget, v2(0.0f, 0.0f), screenDim, entry->color);
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_BITMAP) {
      struct render_group_entry_bitmap *entry = data;
      pushBufferIndex += sizeof(*entry);

      assert(entry->bitmap);

      struct render_entity_basis_p_result basis = GetRenderEntityBasisP(&entry->basis, renderGroup, screenDim);
      if (!basis.valid || basis.scale <= 0.0f)
        continue;

#if 0
      DrawBitmap(outputTarget, entry->bitmap, basis.p, entry->alpha);
#else
      DrawRectangleHopefullyQuickly(outputTarget, basis.p, v2_mul(v2(entry->size.x, 0), basis.scale),
                                    v2_mul(v2(0, entry->size.y), basis.scale), v4(1.0f, 1.0f, 1.0f, entry->alpha),
                                    entry->bitmap, pixelsToMeters);
#endif
    }

    else if (header->type & RENDER_GROUP_ENTRY_TYPE_RECTANGLE) {
      struct render_group_entry_rectangle *entry = data;
      pushBufferIndex += sizeof(*entry);

      struct render_entity_basis_p_result basis = GetRenderEntityBasisP(&entry->basis, renderGroup, screenDim);
      if (!basis.valid || basis.scale <= 0.0f)
        continue;
      DrawRectangle(outputTarget, basis.p, v2_add(basis.p, v2_mul(entry->dim, basis.scale)), entry->color);
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

  END_TIMER_BLOCK(DrawRenderGroup);
}
