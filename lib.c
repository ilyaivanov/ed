#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <windows.h>
//  #include "common.c"

typedef uint64_t u64;
typedef int64_t i64;

typedef uint32_t u32;
typedef int32_t i32;
typedef uint8_t u8;
typedef float f32;
typedef double f64;

typedef struct MyBitmap {
  i32 width;
  i32 height;
  i32 bytesPerPixel;
  u32* pixels;
} MyBitmap;

typedef struct {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
} Rect;

int _DllMainCRTStartup(HINSTANCE const instance, DWORD const reason, void* const reserved) {
  return 1;
}

void PaintRect(MyBitmap* canvas, i32 x, i32 y, i32 width, i32 height, u32 color) {
  i32 x0 = x < 0 ? 0 : x;
  i32 y0 = y < 0 ? 0 : y;
  i32 x1 = (x + width) > canvas->width ? canvas->width : (x + width);
  i32 y1 = (y + height) > canvas->height ? canvas->height : (y + height);

  for (i32 j = y0; j < y1; j++) {
    for (i32 i = x0; i < x1; i++) {
      canvas->pixels[j * canvas->width + i] = color;
    }
  }
}

typedef void Foo();

__declspec(dllexport) void RenderApp(MyBitmap* canvas, Rect* rect) {
  Foo* f = 0;
  f();
  u32 c1 = 0x445588;
  PaintRect(canvas, rect->x, rect->y, rect->width / 2, rect->height, c1);

  u32 c2 = 0x224422;
  PaintRect(canvas, rect->x + rect->width / 2, rect->y, rect->width / 2, rect->height, c2);
}