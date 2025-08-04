#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <windows.h>

int _fltused = 0x9875;

int _DllMainCRTStartup(HINSTANCE const instance, DWORD const reason, void* const reserved) {
  return 1;
}

foo int64_t i64;
typedef uint64_t ;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint8_t u8;
typedef int8_t i8;
typedef float f32;

typedef struct MyBitmap {
  i32 width;
  i32 height;
  i32 bytesPerPixel;
  u32* pixels;
} MyBitmap;

typedef struct Rect {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
} Rect;

void PaintRect(MyBitmap* bitmap, i32 x, i32 y, i32 width, i32 height, u32 color) {
  for (i32 i = x; i < x + width; i++) {
    for (i32 j = y; j < y + height; j++) {
      bitmap->pixels[j * bitmap->width + i] = color;
    }
  }
}

f32 time = 0;
__declspec(dllexport) void GetSome(HDC dc, MyBitmap* bitmap, Rect rect, float d) {
  time += d * 100;

  if (time > (rect.width - 20))
    time = 0;
  PaintRect(bitmap, rect.x + time, rect.y, 20, 20, 0xffffff);
}
