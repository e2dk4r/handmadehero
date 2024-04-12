#ifndef HANDMADEHERO_MATH_H
#define HANDMADEHERO_MATH_H

#include "types.h"

#define minimum(a, b) (a < b) ? a : b
#define maximum(a, b) (a > b) ? a : b

comptime f32 PI32 = 3.14159274101257324f;

static inline i32
roundf32toi32(f32 value)
{
  return (i32)__builtin_round(value);
}

static inline u32
roundf32tou32(f32 value)
{
  return (u32)__builtin_round(value);
}

static inline i32
truncatef32toi32(f32 value)
{
  return (i32)value;
}

static inline i32
floorf32toi32(f32 value)
{
  return (i32)__builtin_floor(value);
}

static inline i32
ceilf32toi32(f32 value)
{
  return (i32)__builtin_ceilf(value);
}

static inline i32
FindLeastSignificantBitSet(i32 value)
{
  return __builtin_ffs(value) - 1;
}

static inline f32
square(f32 value)
{
  return value * value;
}
static inline f32
absolute(f32 value)
{
  return __builtin_fabsf(value);
}
static inline f32
SquareRoot(f32 value)
{
  return __builtin_sqrtf(value);
}
static inline i32
SignOf(i32 value)
{
  return value >= 0 ? 1 : -1;
}

static inline f32
Sin(f32 value)
{
  return __builtin_sinf(value);
};

struct v2 {
  union {
    struct {
      f32 x;
      f32 y;
    };
    f32 e[2];
  };
};

struct v3 {
  union {
    struct {
      f32 x;
      f32 y;
      f32 z;
    };
    struct {
      f32 r;
      f32 g;
      f32 b;
    };
    struct {
      struct v2 xy;
      f32 _ignored0;
    };
    struct {
      f32 _ignored1;
      struct v2 yz;
    };
    f32 e[3];
  };
};

struct v4 {
  union {
    struct {
      f32 x;
      f32 y;
      f32 z;
      f32 w;
    };
    struct {
      f32 r;
      f32 g;
      f32 b;
      f32 a;
    };
    f32 e[4];
  };
};

/****************************************************************
 * v2 OPERATIONS
 ****************************************************************/

static inline struct v2
v2(f32 x, f32 y)
{
  struct v2 result;

  result.x = x;
  result.y = y;

  return result;
}

static inline void
v2_add_ref(struct v2 *a, struct v2 b)
{
  a->x += b.x;
  a->y += b.y;
}

static inline struct v2
v2_add(struct v2 a, struct v2 b)
{
  struct v2 result = a;
  v2_add_ref(&result, b);
  return result;
}

static inline void
v2_sub_ref(struct v2 *a, struct v2 b)
{
  a->x -= b.x;
  a->y -= b.y;
}

static inline struct v2
v2_sub(struct v2 a, struct v2 b)
{
  struct v2 result = a;
  v2_sub_ref(&result, b);
  return result;
}

static inline void
v2_neg_ref(struct v2 *a)
{
  a->x = -a->x;
  a->y = -a->y;
}

static inline struct v2
v2_neg(struct v2 a)
{
  struct v2 result = a;
  v2_neg_ref(&result);
  return result;
}

static inline void
v2_mul_ref(struct v2 *a, f32 value)
{
  a->x *= value;
  a->y *= value;
}
/* scaler multiplication */
static inline struct v2
v2_mul(struct v2 a, f32 value)
{
  struct v2 result = a;
  v2_mul_ref(&result, value);
  return result;
}

static inline f32
v2_dot(struct v2 a, struct v2 b)
{
  f32 value = a.x * b.x + a.y * b.y;
  return value;
}

static inline f32
v2_length_square(struct v2 a)
{
  f32 value;

  value = v2_dot(a, a);

  return value;
}

static inline f32
v2_length(struct v2 a)
{
  f32 value;
  value = SquareRoot(v2_length_square(a));
  return value;
}

static inline struct v2
v2_hadamard(struct v2 a, struct v2 b)
{
  struct v2 result = {a.x * b.x, a.y * b.y};
  return result;
}

internal inline struct v3
v2_to_v3(struct v2 a, f32 z)
{
  struct v3 result;
  result.xy = a;
  result.z = z;
  return result;
}

/****************************************************************
 * v3 OPERATIONS
 ****************************************************************/

static inline struct v3
v3(f32 x, f32 y, f32 z)
{
  struct v3 result;
  result.x = x;
  result.y = y;
  result.z = z;
  return result;
}

static inline void
v3_add_ref(struct v3 *a, struct v3 b)
{
  a->x += b.x;
  a->y += b.y;
  a->z += b.z;
}

static inline struct v3
v3_add(struct v3 a, struct v3 b)
{
  struct v3 result = a;
  v3_add_ref(&result, b);
  return result;
}

static inline void
v3_sub_ref(struct v3 *a, struct v3 b)
{
  a->x -= b.x;
  a->y -= b.y;
  a->z -= b.z;
}

static inline struct v3
v3_sub(struct v3 a, struct v3 b)
{
  struct v3 result = a;
  v3_sub_ref(&result, b);
  return result;
}

static inline void
v3_neg_ref(struct v3 *a)
{
  a->x = -a->x;
  a->y = -a->y;
  a->z = -a->z;
}

static inline struct v3
v3_neg(struct v3 a)
{
  struct v3 result = a;
  v3_neg_ref(&result);
  return result;
}

static inline void
v3_mul_ref(struct v3 *a, f32 value)
{
  a->x *= value;
  a->y *= value;
  a->z *= value;
}

/* scaler multiplication */
static inline struct v3
v3_mul(struct v3 a, f32 value)
{
  struct v3 result = a;
  v3_mul_ref(&result, value);
  return result;
}

static inline f32
v3_dot(struct v3 a, struct v3 b)
{
  f32 value = a.x * b.x + a.y * b.y + a.z * b.z;
  return value;
}

static inline f32
v3_length_square(struct v3 a)
{
  f32 value;
  value = v3_dot(a, a);
  return value;
}

internal inline f32
v3_length(struct v3 a)
{
  f32 value;
  value = SquareRoot(v3_dot(a, a));
  return value;
}

static inline struct v3
v3_hadamard(struct v3 a, struct v3 b)
{
  struct v3 result = {a.x * b.x, a.y * b.y, a.z * b.z};
  return result;
}

/****************************************************************
 * v4 OPERATIONS
 ****************************************************************/

static inline struct v4
v4(f32 x, f32 y, f32 z, f32 w)
{
  struct v4 result;
  result.x = x;
  result.y = y;
  result.z = z;
  result.w = w;
  return result;
}

static inline void
v4_add_ref(struct v4 *a, struct v4 b)
{
  a->x += b.x;
  a->y += b.y;
  a->z += b.z;
  a->w += b.w;
}

static inline struct v4
v4_add(struct v4 a, struct v4 b)
{
  struct v4 result = a;
  v4_add_ref(&result, b);
  return result;
}

static inline void
v4_sub_ref(struct v4 *a, struct v4 b)
{
  a->x -= b.x;
  a->y -= b.y;
  a->z -= b.z;
  a->w -= b.w;
}

static inline struct v4
v4_sub(struct v4 a, struct v4 b)
{
  struct v4 result = a;
  v4_sub_ref(&result, b);
  return result;
}

static inline void
v4_neg_ref(struct v4 *a)
{
  a->x = -a->x;
  a->y = -a->y;
  a->z = -a->z;
  a->w = -a->w;
}

static inline struct v4
v4_neg(struct v4 a)
{
  struct v4 result = a;
  v4_neg_ref(&result);
  return result;
}

static inline void
v4_mul_ref(struct v4 *a, f32 value)
{
  a->x *= value;
  a->y *= value;
  a->z *= value;
  a->w *= value;
}

/* scaler multiplication */
static inline struct v4
v4_mul(struct v4 a, f32 value)
{
  struct v4 result = a;
  v4_mul_ref(&result, value);
  return result;
}

static inline f32
v4_dot(struct v4 a, struct v4 b)
{
  f32 value = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  return value;
}

static inline f32
v4_length_square(struct v4 a)
{
  f32 value;
  value = v4_dot(a, a);
  return value;
}

/****************************************************************
 * rect OPERATIONS
 ****************************************************************/

struct rect {
  struct v3 min;
  struct v3 max;
};

static inline struct rect
RectMinMax(struct v3 min, struct v3 max)
{
  return (struct rect){
      .min = min,
      .max = max,
  };
}

static inline struct rect
RectMinDim(struct v3 min, struct v3 dim)
{
  return (struct rect){
      .min = min,
      .max = v3_add(min, dim),
  };
}

static inline struct rect
RectCenterHalfDim(struct v3 center, struct v3 halfDim)
{
  return (struct rect){
      .min = v3_sub(center, halfDim),
      .max = v3_add(center, halfDim),
  };
}

static inline struct rect
RectCenterDim(struct v3 center, struct v3 dim)
{
  return RectCenterHalfDim(center, v3_mul(dim, 0.5f));
}

static inline u8
RectIsPointInside(struct rect rect, struct v3 testPoint)
{
  return
      /* x boundries */
      testPoint.x >= rect.min.x &&
      testPoint.x < rect.max.x
      /* y boundries */
      && testPoint.y >= rect.min.y &&
      testPoint.y < rect.max.y
      /* z boundries */
      && testPoint.z >= rect.min.z && testPoint.z < rect.max.z;
}

static inline struct v3
RectMin(struct rect *rect)
{
  return rect->min;
}

static inline struct v3
RectMax(struct rect *rect)
{
  return rect->max;
}

static inline struct v3
RectCenter(struct rect *rect)
{
  return v3_mul(v3_add(rect->min, rect->max), 0.5f);
}

internal inline struct rect
RectAddRadius(struct rect *rect, struct v3 radius)
{
  struct rect result;

  result.min = v3_sub(rect->min, radius);
  result.max = v3_add(rect->max, radius);

  return result;
}

#endif /* HANDMADEHERO_MATH_H */
