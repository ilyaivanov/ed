#pragma once
#define WIN32_LEAN_AND_MEAN
#include <dwmapi.h>
#include <stdint.h>
#include <windows.h>
#include <winuser.h>

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
  u32 *pixels;
} MyBitmap;
typedef struct {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
} Rect;
#define ArrayLength(array) (sizeof(array) / sizeof(array[0]))
#define Assert(cond)                                                           \
  if (!(cond)) {                                                               \
    *(u32 *)0 = 0;                                                             \
  }

#define Fail(msg) Assert(0)
inline void *VirtualAllocateMemory(size_t size) {
  return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
};

inline void VirtualFreeMemory(void *ptr) { VirtualFree(ptr, 0, MEM_RELEASE); };

HWND OpenWindow(WNDPROC OnEvent, u32 bgColor, char *title) {
  HINSTANCE instance = GetModuleHandle(0);
  WNDCLASSA windowClass = {0};
  windowClass.hInstance = instance;
  windowClass.lpfnWndProc = OnEvent;
  windowClass.lpszClassName = "MyWindow";
  windowClass.style = CS_VREDRAW | CS_HREDRAW;
  windowClass.hCursor = LoadCursor(0, IDC_ARROW);
  // not using COLOR_WINDOW + 1 because it doesn't fucking work
  // this line fixes a flash of a white background for 1-2 frames during start
  u32 c = ((bgColor & 0xff) << 16) | (bgColor & 0x00ff00) |
          ((bgColor & 0xff0000) >> 16);
  windowClass.hbrBackground = CreateSolidBrush(c);
  // };
  RegisterClassA(&windowClass);

  HDC dc = GetDC(0);
  int screenWidth = GetDeviceCaps(dc, HORZRES);
  // int screenHeight = GetDeviceCaps(dc, VERTRES);

  int windowWidth = 1800;
  int windowHeight = 1200;

  HWND window = CreateWindowA(windowClass.lpszClassName, title,
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              screenWidth - windowWidth + 11, 0, windowWidth,
                              windowHeight, 0, 0, instance, 0);

  BOOL USE_DARK_MODE = TRUE;
  SUCCEEDED(DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE,
                                  &USE_DARK_MODE, sizeof(USE_DARK_MODE)));

  return window;
}

typedef BOOL WINAPI set_process_dpi_aware(void);
typedef BOOL WINAPI set_process_dpi_awareness_context(DPI_AWARENESS_CONTEXT);
static void PreventWindowsDPIScaling() {
  HMODULE WinUser = LoadLibraryW(L"user32.dll");
  set_process_dpi_awareness_context *SetProcessDPIAwarenessContext =
      (set_process_dpi_awareness_context *)GetProcAddress(
          WinUser, "SetProcessDPIAwarenessContext");
  if (SetProcessDPIAwarenessContext) {
    SetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
  } else {
    set_process_dpi_aware *SetProcessDPIAware =
        (set_process_dpi_aware *)GetProcAddress(WinUser, "SetProcessDPIAware");
    if (SetProcessDPIAware) {
      SetProcessDPIAware();
    }
  }
}

inline u8 RoundU8(f32 v) { return (u8)(v + 0.5); }
inline f32 lerp(f32 from, f32 to, f32 factor) {
  return from * (1 - factor) + to * factor;
}

i64 GetMyFileSize(char *path) {
  HANDLE file =
      CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  LARGE_INTEGER size;
  GetFileSizeEx(file, &size);

  CloseHandle(file);
  return (i64)size.QuadPart;
}

void ReadFileInto(char *path, u32 fileSize, char *buffer) {
  HANDLE file =
      CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  DWORD bytesRead;
  ReadFile(file, buffer, fileSize, &bytesRead, 0);
  CloseHandle(file);
}
void WriteMyFile(char *path, char *content, int size)
{
    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    DWORD bytesWritten;
    int res = WriteFile(file, content, size, &bytesWritten, 0);
    CloseHandle(file);

   // Assert(bytesWritten == size);
}
inline i64 GetPerfFrequency()
{
    LARGE_INTEGER res;
    QueryPerformanceFrequency(&res);
    return res.QuadPart;
}

inline i64 GetPerfCounter()
{
    LARGE_INTEGER res;
    QueryPerformanceCounter(&res);
    return res.QuadPart;
}
