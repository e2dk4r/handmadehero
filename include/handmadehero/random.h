#ifndef HANDMADEHERO_RANDOM_H
#define HANDMADEHERO_RANDOM_H

#include "types.h"

struct random_series {
  u32 randomNumberIndex;
};

struct random_series
RandomSeed(u32 value);

// [0, U32_MAX]
u32
RandomNumber(struct random_series *series);

// [0, choiceCount)
u32
RandomChoice(struct random_series *series, u32 choiceCount);

// [0, 1]
f32
RandomNormal(struct random_series *series);

// [-1, 1]
f32
RandomUnit(struct random_series *series);

// [min, max]
f32
RandomBetween(struct random_series *series, f32 min, f32 max);

// [min, max]
s32
RandomBetweens32(struct random_series *series, s32 min, s32 max);

#endif /* HANDMADEHERO_RANDOM_H */
