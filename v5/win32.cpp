#pragma once
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <dwmapi.h>
#include <stdint.h>
#include <windows.h>

int _fltused = 0x9875;
typedef char c8;
typedef wchar_t c16;
typedef int8_t i8;
typedef uint8_t u8;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef uint16_t u16;
typedef float f32;
typedef double f64;

#define ArrayLength(array) (sizeof(array) / sizeof(array[0]))
#define KB(v) (1024 * v)

inline void* valloc(size_t size) {
  return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
};

inline void vfree(void* ptr) {
  VirtualFree(ptr, 0, MEM_RELEASE);
};

int strequal(const char* a, const char* b) {
  while (*a && *b) {
    if (*a != *b)
      return 0;
    a++;
    b++;
  }
  return *a == *b;
}

int strequalw(const u16* a, const u16* b) {
  while (*a && *b) {
    if (*a != *b)
      return 0;
    a++;
    b++;
  }
  return *a == *b;
}

u32 ToWinColor(u32 color) {
  return ((color & 0xff0000) >> 16) | (color & 0x00ff00) | ((color & 0x0000ff) << 16);
}

void TextColors(HDC dc, u32 foreground, u32 background) {
  SetBkColor(dc, ToWinColor(background));
  SetTextColor(dc, ToWinColor(foreground));
}

// Increasing Read Bandwidth with SIMD Instructions
// https://www.computerenhance.com/p/increasing-read-bandwidth-with-simd
#pragma function(memset)
void* memset(void* dest, int c, size_t count) {
  char* bytes = (char*)dest;
  while (count--) {
    *bytes++ = (char)c;
  }
  return dest;
}

typedef struct CharBuffer {
  char content[200];
  i32 size;
} CharBuffer;

void AppendStr(CharBuffer* buff, char* str) {
  i32 l = 0;
  while (str[l] != '\0') {
    buff->content[buff->size++] = str[l++];
  }
}

void AppendNum(CharBuffer* buff, int v) {
  char temp[20] = {0};
  i32 l = 0;

  if (v == 0) {
    temp[l++] = '0';
  }
  if (v < 0) {
    buff->content[buff->size++] = '-';
    v = -v;
  }

  while (v > 0) {
    temp[l++] = '0' + (v % 10);
    v /= 10;
  }

  for (i32 i = l - 1; i >= 0; i--)
    buff->content[buff->size + l - i - 1] = temp[i];

  buff->size += l;
}
typedef LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND OpenWindow(WindowProc* proc) {
  HINSTANCE instance = GetModuleHandle(0);
  WNDCLASSW windowClass = {};
  windowClass.hInstance = instance;
  windowClass.lpfnWndProc = proc;
  windowClass.lpszClassName = L"MyClassName";
  windowClass.style = CS_VREDRAW | CS_HREDRAW;
  windowClass.hCursor = LoadCursor(0, IDC_ARROW);
  windowClass.hbrBackground = CreateSolidBrush(0);

  RegisterClassW(&windowClass);

  HDC screenDc = GetDC(0);

  int screenWidth = GetDeviceCaps(screenDc, HORZRES);
  //  int screenHeight = GetDeviceCaps(screenDc, VERTRES);

  int width = 1800;
  int height = 2050;

  HWND win = CreateWindowW(windowClass.lpszClassName, L"Hello", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           screenWidth - width, 0, width, height, 0, 0, instance, 0);

  ShowWindow(win, SW_SHOW);
  BOOL USE_DARK_MODE = TRUE;
  SUCCEEDED(DwmSetWindowAttribute(win, DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE,
                                  sizeof(USE_DARK_MODE)));
  return win;
}

inline BOOL IsKeyPressed(u32 code) {
  return (GetKeyState(code) >> 15) & 1;
}

i64 GetMyFileSize(const wchar_t* path) {
  HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  LARGE_INTEGER size = {};
  GetFileSizeEx(file, &size);

  CloseHandle(file);
  return (i64)size.QuadPart;
}

void ReadFileInto(const wchar_t* path, u32 fileSize, char* buffer) {
  HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  DWORD bytesRead;
  ReadFile(file, buffer, fileSize, &bytesRead, 0);
  CloseHandle(file);
}

//
// Math
//

i32 roundff(f32 v) {
  return (i32)(v + 0.5f);
}

i32 Max(i32 a, i32 b) {
  if (a > b)
    return a;
  else
    return b;
}

i32 Min(i32 a, i32 b) {
  if (a < b)
    return a;
  else
    return b;
}

f32 lerp(f32 from, f32 to, f32 v) {
  return (1 - v) * from + to * v;
}

//
// Text
//

u32 IsPunctuation(char ch) {
  // use a lookup table
  const char* punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

  const char* p = punctuation;
  while (*p) {
    if (ch == *p) {
      return 1;
    }
    p++;
  }
  return 0;
}

u32 IsWhitespace(char ch) {
  return ch == ' ' || ch == '\n';
}

u32 IsNumeric(char ch) {
  return (ch >= '0' && ch <= '9');
}

u32 IsAlphaNumeric(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

u32 IsAsciiChar(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool IsPrintable(char ch) {
  return ch >= ' ' && ch <= '}';
}

i32 StrLen(const wchar_t* str) {
  i32 len = 0;
  while (str[len] != L'\0')
    len++;

  return len;
}

i32 Appendw(c16* buff, i32 len, const c16* str) {
  i32 i = 0;
  while (str[i] != 0) {
    buff[len] = str[i];
    len++;
    i++;
  }

  return len;
}

//
// Painting
//

void InitBitmapInfo(BITMAPINFO* bitmapInfo, i32 width, i32 height) {
  bitmapInfo->bmiHeader.biSize = sizeof(bitmapInfo->bmiHeader);
  bitmapInfo->bmiHeader.biBitCount = 32;
  bitmapInfo->bmiHeader.biWidth = width;
  bitmapInfo->bmiHeader.biHeight = -height; // makes rows go up, instead of going down by default
  bitmapInfo->bmiHeader.biPlanes = 1;
  bitmapInfo->bmiHeader.biCompression = BI_RGB;
}

typedef struct Rect {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
} Rect;

typedef struct MyBitmap {
  i32 width;
  i32 height;
  i32 bytesPerPixel;
  u32* pixels;
} MyBitmap;

void PaintRect(MyBitmap* canvas, f32 x, f32 y, f32 width, f32 height, u32 color) {
  int startX = roundff(x);
  int startY = roundff(y);
  int endX = roundff(x + width);
  int endY = roundff(y + height);

  startX = Max(startX, 0);
  startY = Max(startY, 0);

  endX = Min(endX, canvas->width);
  endY = Min(endY, canvas->height);

  for (i32 y = startY; y < endY; y++) {
    for (i32 x = startX; x < endX; x++) {
      canvas->pixels[x + y * canvas->width] = color;
    }
  }
}

void PaintSquareCentered(MyBitmap* canvas, f32 x, f32 y, f32 size, u32 color) {
  PaintRect(canvas, x - size / 2.0f, y - size / 2.0f, size, size, color);
}
