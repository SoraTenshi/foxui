#ifndef FOXUI_PLATFORM_H
#define FOXUI_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float   f32;
typedef double  f64;

typedef uintptr_t usize;

typedef struct String8 {
    u8   *items;
    usize count;
} String8;

#ifdef __cplusplus
#define S8(s) (String8{(u8 *)(s), sizeof(s) - 1})
#else
#define S8(s) ((String8){(u8 *)(s), sizeof(s) - 1})
#endif

#define STR_FMT "%.*s"
#define STR_FORMAT(str) (s32)((str).count), (char const *)((str).items)

#define FOXUI_ARRAY_COUNT(arr) (sizeof((arr)) / sizeof((arr)[0]))

#define FOXUI_INTERNAL static
#define FOXUI_GLOBAL   static

#endif /* FOXUI_PLATFORM_H */