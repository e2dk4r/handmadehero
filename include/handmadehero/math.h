#ifndef HANDMADEHERO_MATH_H
#define HANDMADEHERO_MATH_H

#include "types.h"

static inline i32 roundf32toi32(f32 value) {
  return (i32)__builtin_round(value);
}

static inline u32 roundf32tou32(f32 value) {
  return (u32)__builtin_round(value);
}

static inline i32 truncatef32toi32(f32 value) { return (i32)value; }

static inline i32 floorf32toi32(f32 value) {
  return (i32)__builtin_floor(value);
}

#endif /* HANDMADEHERO_MATH_H */
