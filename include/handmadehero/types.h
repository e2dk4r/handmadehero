#ifndef HANDMADEHERO_TYPES_H
#define HANDMADEHERO_TYPES_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

#define I8_MIN (-128)
#define I16_MIN (-32767 - 1)
#define I32_MIN (-2147483647 - 1)
#define I64_MIN (-9223372036854775807L - 1)

#define I8_MAX (127)
#define I16_MAX (32767)
#define I32_MAX (2147483647)
#define I64_MAX (9223372036854775807L)

#define U8_MAX (255)
#define U16_MAX (65535)
#define U32_MAX (4294967295U)
#define U64_MAX (18446744073709551615UL)

#define F32_MIN __FLT_MIN__;
#define F32_MAX __FLT_MAX__;

#define comptime static const
#define internal static

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

comptime u64 KILOBYTES = 1 << 10;
comptime u64 MEGABYTES = 1 << 20;
comptime u64 GIGABYTES = 1 << 30;

#endif /* HANDMADEHERO_TYPES_H */
