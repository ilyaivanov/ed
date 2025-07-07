#pragma once
#include "primitives.h"
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#include <winuser.h>

typedef struct MyBitmap {
  i32 width;
  i32 height;
  i32 bytesPerPixel;
  u32* pixels;
} MyBitmap;

typedef struct Window {
  i32 left;
  i32 top;

  i32 width;
  i32 height;
  u32 bg;
  const char* title;
  i32 isFullscreen;
  i32 isTerminated;
  MyBitmap canvas;

  // win32 specific
  BITMAPINFO bitmapInfo;
  HDC dc;
  HWND windowHandle;
  WNDPROC onEvent;
} Window;

#define ArrayLength(array) (sizeof(array) / sizeof(array[0]))
inline void* VirtualAllocateMemory(size_t size) {
  return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
};

inline void VirtualFreeMemory(void* ptr) {
  VirtualFree(ptr, 0, MEM_RELEASE);
};

typedef BOOL WINAPI set_process_dpi_aware(void);
typedef BOOL WINAPI set_process_dpi_awareness_context(DPI_AWARENESS_CONTEXT);
static void PreventWindowsDPIScaling() {
  HMODULE WinUser = LoadLibraryW(L"user32.dll");
  set_process_dpi_awareness_context* SetProcessDPIAwarenessContext =
      (set_process_dpi_awareness_context*)GetProcAddress(WinUser, "SetProcessDPIAwarenessContext");
  if (SetProcessDPIAwarenessContext) {
    SetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
  } else {
    set_process_dpi_aware* SetProcessDPIAware =
        (set_process_dpi_aware*)GetProcAddress(WinUser, "SetProcessDPIAware");
    if (SetProcessDPIAware) {
      SetProcessDPIAware();
    }
  }
}

WINDOWPLACEMENT prevWindowDimensions = {sizeof(prevWindowDimensions)};
void SetFullscreen(HWND window, i32 isFullscreen) {
  DWORD style = GetWindowLong(window, GWL_STYLE);
  if (isFullscreen) {
    MONITORINFO monitorInfo = {sizeof(monitorInfo)};
    if (GetWindowPlacement(window, &prevWindowDimensions) &&
        GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo)) {
      SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);

      SetWindowPos(window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                   monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                   monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
  } else {
    SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(window, &prevWindowDimensions);
    SetWindowPos(window, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
}

void OpenWindow(Window* window) {
  HINSTANCE instance = GetModuleHandle(0);
  WNDCLASSA windowClass = {0};
  windowClass.hInstance = instance;
  windowClass.lpfnWndProc = window->onEvent;
  windowClass.lpszClassName = "MyClassName";
  windowClass.style = CS_VREDRAW | CS_HREDRAW;
  windowClass.hCursor = LoadCursor(0, IDC_ARROW);
  // not using COLOR_WINDOW + 1 because it doesn't fucking work
  // this line fixes a flash of a white background for 1-2 frames during start
  // u32 c = ((bgColor & 0xff) << 16) | (bgColor & 0x00ff00) | ((bgColor &
  // 0xff0000) >> 16);
  windowClass.hbrBackground = CreateSolidBrush(window->bg);
  // };
  RegisterClassA(&windowClass);

  HDC screenDc = GetDC(0);

  int screenWidth = GetDeviceCaps(screenDc, HORZRES);
  int screenHeight = GetDeviceCaps(screenDc, VERTRES);

  // int windowWidth = 1800;
  // int windowHeight = screenHeight;

  window->windowHandle = CreateWindowA(
      windowClass.lpszClassName, window->title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
      screenWidth - window->width, 0, window->width, window->height, 0, 0, instance, 0);

  if (window->isFullscreen)
    SetFullscreen(window->windowHandle, 1);

  window->dc = GetDC(window->windowHandle);
  ShowWindow(window->windowHandle, SW_SHOW);
  BOOL USE_DARK_MODE = TRUE;
  SUCCEEDED(DwmSetWindowAttribute(window->windowHandle, DWMWA_USE_IMMERSIVE_DARK_MODE,
                                  &USE_DARK_MODE, sizeof(USE_DARK_MODE)));
}

void OnSize(Window* window, LPARAM lParam) {

  window->width = LOWORD(lParam);
  window->height = HIWORD(lParam);

  i32 canvasWidth = window->width;
  i32 canvasHeight = window->height;
  window->canvas.width = canvasWidth;
  window->canvas.height = canvasHeight;
  window->bitmapInfo.bmiHeader.biSize = sizeof(window->bitmapInfo.bmiHeader);
  window->bitmapInfo.bmiHeader.biBitCount = 32;
  window->bitmapInfo.bmiHeader.biWidth = canvasWidth;
  // window->// makes rows go down, instead of going up by default
  window->bitmapInfo.bmiHeader.biHeight = -canvasHeight;
  window->bitmapInfo.bmiHeader.biPlanes = 1;
  window->bitmapInfo.bmiHeader.biCompression = BI_RGB;

  if (window->canvas.pixels)
    VirtualFreeMemory(window->canvas.pixels);

  window->canvas.pixels = (u32*)VirtualAllocateMemory(canvasWidth * canvasHeight * 4);
  window->dc = GetDC(window->windowHandle);
}

void PaintWindow(Window* window) {
  StretchDIBits(window->dc, 0, 0, window->width, window->height, 0, 0, window->canvas.width,
                window->canvas.height, window->canvas.pixels, &window->bitmapInfo, DIB_RGB_COLORS,
                SRCCOPY);
}
inline i64 GetPerfFrequency() {
  LARGE_INTEGER res;
  QueryPerformanceFrequency(&res);
  return res.QuadPart;
}

inline i64 GetPerfCounter() {
  LARGE_INTEGER res;
  QueryPerformanceCounter(&res);
  return res.QuadPart;
}

void Win32InitOpenGL(Window window) {
  HDC WindowDC = window.dc;
  PIXELFORMATDESCRIPTOR DesiredPixelFormat = {0};
  DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
  DesiredPixelFormat.nVersion = 1;
  DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
  DesiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
  DesiredPixelFormat.cColorBits = 32;
  DesiredPixelFormat.cAlphaBits = 8;
  DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

  int SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
  PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
  DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex, sizeof(SuggestedPixelFormat),
                      &SuggestedPixelFormat);
  SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);

  HGLRC OpenGLRC = wglCreateContext(WindowDC);
  if (!wglMakeCurrent(WindowDC, OpenGLRC)) {

    MessageBox(window.windowHandle, "Failed to initialize OpenGL", "Error", MB_ICONERROR);
  }
  // ReleaseDC(Window, WindowDC);
}

i64 GetMyFileSize(const char* path) {
  HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  LARGE_INTEGER size = {0};
  GetFileSizeEx(file, &size);

  CloseHandle(file);
  return (i64)size.QuadPart;
}

void ReadFileInto(const char* path, u32 fileSize, char* buffer) {
  HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  DWORD bytesRead;
  ReadFile(file, buffer, fileSize, &bytesRead, 0);
  CloseHandle(file);
}

inline BOOL IsKeyPressed(u32 code) {
  return (GetKeyState(code) >> 15) & 1;
}
