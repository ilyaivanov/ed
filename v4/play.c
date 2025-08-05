#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <windows.h>

int _fltused = 0x9875;

int _DllMainCRTStartup(HINSTANCE const instance, DWORD const reason, void* const reserved) {
  return 1;
}

typedef int64_t i64;
typedef uint64_t u64;
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
    
  f32 width = rect.width / 10;
  u32 colors[] = {0xff2222, 0x22ff22, 0x2222ff};
  for (i32 i = 0; i < 10; i++)
    PaintRect(bitmap, rect.x + i * width, rect.y, width, 20, colors[i % 3]);
}
