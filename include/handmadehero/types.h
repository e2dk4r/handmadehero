#ifndef HANDMADEHERO_TYPES_H
#define HANDMADEHERO_TYPES_H

typedef __UINT8_TYPE__ u8;
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;

typedef __INT8_TYPE__ s8;
typedef __INT16_TYPE__ s16;
typedef __INT32_TYPE__ s32;
typedef __INT64_TYPE__ s64;

typedef __INTPTR_TYPE__ uptr;

typedef float f32;
typedef double f64;

typedef u32 b32;

#define I8_MIN (-128)
#define I16_MIN (-32767 - 1)
#define I32_MIN (-2147483647 - 1)
#define I64_MIN (-9223372036854775807L - 1)

#define S8_MAX (127)
#define S16_MAX (32767)
#define S32_MAX (2147483647)
#define S64_MAX (9223372036854775807L)

#define U8_MAX (255)
#define U16_MAX (65535)
#define U32_MAX (4294967295U)
#define U64_MAX (18446744073709551615UL)

#define F32_MIN __FLT_MIN__;
#define F32_MAX __FLT_MAX__;

#define comptime static const
#define internal static
#define global_variable static

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

#define ALIGN(value, alignment) (((value) + (alignment - 1)) & (__typeof__(value))~(alignment - 1))
#define ALIGN4(value) ALIGN(value, 4)
#define ALIGN8(value) ALIGN(value, 8)
#define ALIGN16(value) ALIGN(value, 16)
#define IS_ALIGNED(value, alignment) (((value) & (__typeof__(value))(alignment - 1)) == 0)

#endif /* HANDMADEHERO_TYPES_H */
