#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <windows.h>

int _DllMainCRTStartup(HINSTANCE const instance, DWORD const reason, void* const reserved) {
  return 1;
}

typedef int64_t i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint8_t u8;
typedef int8_t i8;

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

__declspec(dllexport) void GetSome(MyBitmap* bitmap, Rect rect, float d) {
  u32 colors[] = {0xff2222, 0x22ff22, 0x2222ff};

  int x = rect.x;
  int y = rect.y;
  int width = rect.width / 10;
  int height = rect.height / 5;
  for (i32 i = 0; i < 10; i++) {
    PaintRect(bitmap, x, y, width, height, colors[i % 4]);
    x += width;
  }
}
