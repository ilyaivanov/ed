
#pragma once
#include <stdint.h>

typedef uint64_t u64;
typedef int64_t i64;

typedef uint32_t u32;
typedef int32_t i32;

typedef uint16_t u16;
typedef int16_t i16;

typedef uint8_t u8;
typedef float f32;
typedef double f64;

typedef struct V2 {
  f32 x;
  f32 y;
} V2;


typedef union V3 {
  struct { f32 x; f32 y; f32 z; };
  struct { f32 r; f32 g; f32 b; };
  struct { V2 xy; f32 ignored0; };
} V3;

typedef union V4 {
  struct { f32 x; f32 y; f32 z; f32 w; };
  struct { f32 r; f32 g; f32 b; f32 a; };
  struct { V2 xy; f32 ignored0; f32 ignored1; };
  struct { V3 xyz; f32 ignored2;  };
} V4;

typedef union M4{
  V4 cols[4];
  f32 e[16];
} M4;

typedef union M2{
  V2 cols[2];
  f32 e[8];
} M2;

typedef union M3{
  V3 cols[3];
  f32 e[12];
} M3;

typedef struct Rect {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
} Rect;
typedef V2 v2;
typedef V3 v3;
typedef V4 v4;
typedef M4 m4;
