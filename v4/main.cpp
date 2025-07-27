#define WIN32_LEAN_AND_MEAN
#include <dwmapi.h>
#include <stdint.h>
#include <windows.h>

extern "C" int _fltused = 0x9875;

int isRunning = 1;
int isF = 0;

typedef int64_t i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef int32_t i32;
typedef float f32;
typedef wchar_t wc;

enum Mode { Normal, Insert };

typedef struct MyBitmap {
  i32 width;
  i32 height;
  i32 bytesPerPixel;
  u32* pixels;
} MyBitmap;

BITMAPINFO bitmapInfo;
MyBitmap canvas;
HBITMAP canvasBitmap;
i32 width;
i32 height;

i32 pos = 0;
i32 desiredOffset = 0;
HDC dc;
HWND win;
Mode mode;
char* content;
i64 size;
i64 capacity;

#define ArrayLength(array) (sizeof(array) / sizeof(array[0]))
inline void* valloc(size_t size) {
  return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
};

inline void vfree(void* ptr) {
  VirtualFree(ptr, 0, MEM_RELEASE);
};

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

// TODO: no need to allocate memory each call, pre-allocate memory and check it's capacity
#pragma function(memmove)
void myMemMove(void* dest, void* src, size_t n) {
  char* csrc = (char*)src;
  char* cdest = (char*)dest;

  // Create a temporary array to hold data of src
  char* temp = (char*)valloc(n);

  // Copy data from csrc[] to temp[]
  for (int i = 0; i < n; i++)
    temp[i] = csrc[i];

  // Copy data from temp[] to cdest[]
  for (int i = 0; i < n; i++)
    cdest[i] = temp[i];

  vfree(temp);
}

extern "C" void* memcpy(void* dst, const void* src, size_t n) {
  unsigned char* d = (unsigned char*)dst;
  const unsigned char* s = (const unsigned char*)src;
  while (n--)
    *d++ = *s++;
  return dst;
}

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

// https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
WINDOWPLACEMENT prevWindowDimensions = {sizeof(prevWindowDimensions)};
void SetFullscreen(HWND window, int isFullscreen) {
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

void WriteMyFile(char* path, char* content, int size) {
  HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

  DWORD bytesWritten;
  int res = WriteFile(file, content, size, &bytesWritten, 0);
  CloseHandle(file);
}

u32 ToWinColor(u32 color) {
  return ((color & 0xff0000) >> 16) | (color & 0x00ff00) | ((color & 0x0000ff) << 16);
}

void TextColors(u32 foreground, u32 background) {
  SetBkColor(dc, ToWinColor(background));
  SetTextColor(dc, ToWinColor(foreground));
}

void InitBitmapInfo(BITMAPINFO* bitmapInfo, i32 width, i32 height) {
  bitmapInfo->bmiHeader.biSize = sizeof(bitmapInfo->bmiHeader);
  bitmapInfo->bmiHeader.biBitCount = 32;
  bitmapInfo->bmiHeader.biWidth = width;
  bitmapInfo->bmiHeader.biHeight = -height; // makes rows go up, instead of going down by default
  bitmapInfo->bmiHeader.biPlanes = 1;
  bitmapInfo->bmiHeader.biCompression = BI_RGB;
}

i32 round(f32 v) {
  return (i32)(v + 0.5f);
}

i32 Max(i32 a, i32 b) {
  if (a > b)
    return a;
  return b;
}

i32 Min(i32 a, i32 b) {
  if (a < b)
    return a;
  return b;
}

void PaintRect(f32 x, f32 y, f32 width, f32 height, u32 color) {
  int startX = round(x);
  int startY = round(y);
  int endX = round(x + width);
  int endY = round(y + height);

  startX = Max(startX, 0);
  startY = Max(startY, 0);

  endX = Min(endX, canvas.width);
  endY = Min(endY, canvas.height);

  for (i32 y = startY; y < endY; y++) {
    for (i32 x = startX; x < endX; x++) {
      canvas.pixels[x + y * canvas.width] = color;
    }
  }
}

void Size(LPARAM lParam) {
  width = LOWORD(lParam);
  height = HIWORD(lParam);
  InitBitmapInfo(&bitmapInfo, width, height);
  if (canvasBitmap)
    DeleteObject(canvasBitmap);

  canvasBitmap = CreateDIBSection(dc, &bitmapInfo, DIB_RGB_COLORS, (void**)&canvas.pixels, 0, 0);
  canvas.width = width;
  canvas.height = height;
  canvas.bytesPerPixel = 4;

  SelectObject(dc, canvasBitmap);
}

void DoubleCapacityIfFull() {
  char* currentStr = content;
  capacity = (capacity == 0) ? 4 : (capacity * 2);
  content = (char*)valloc(capacity);
  if (currentStr) {
    myMemMove(content, currentStr, size);
    vfree(currentStr);
  }
}

void BufferRemoveChars(int from, int to) {
  int num_to_shift = size - (to + 1);

  myMemMove(content + from, content + to + 1, num_to_shift);

  size -= (to - from + 1);
}

void BufferInsertChars(char* chars, i32 len, i32 at) {
  while (size + len >= capacity)
    DoubleCapacityIfFull();

  if (size != 0) {
    size += len;

    char* from = content + at;
    char* to = content + at + len;
    myMemMove(to, from, size - at);
  } else {
    size = len;
  }

  for (i32 i = at; i < at + len; i++) {
    content[i] = chars[i - at];
  }
}

i32 FindLineStart(i32 p) {
  while (p > 0 && content[p - 1] != '\n')
    p--;

  return p;
}

i32 FindLineEnd(i32 p) {
  while (p < size && content[p] != '\n')
    p++;

  return p;
}

i32 GetOffset(i32 p) {
  i32 start = FindLineStart(pos);
  i32 off = start;
  while (content[off] == ' ')
    off++;

  return off - start;
}

i32 ApplyDesiredOffset(i32 pos) {
  i32 lineEnd = FindLineEnd(pos);

  return Min(pos + desiredOffset, lineEnd);
}

void UpdateDesiredOffset() {
  desiredOffset = pos - FindLineStart(pos);
}

void UpdateCursorWithoutDesiredOffset(i32 p) {
  pos = Max(Min(p, size), 0);
}

void UpdateCursor(i32 p) {
  UpdateCursorWithoutDesiredOffset(p);
  UpdateDesiredOffset();
}

void MoveDown() {
  i32 end = FindLineEnd(pos);
  UpdateCursorWithoutDesiredOffset(ApplyDesiredOffset(end + 1));
}

void MoveUp() {
  i32 prevLineStart = FindLineStart(FindLineStart(pos) - 1);
  if (prevLineStart < 0)
    prevLineStart = 0;

  UpdateCursorWithoutDesiredOffset(ApplyDesiredOffset(prevLineStart));
}

void MoveLeft() {
  UpdateCursor(pos - 1);
  UpdateDesiredOffset();
}

void MoveRight() {
  UpdateCursor(pos + 1);
  UpdateDesiredOffset();
}

inline BOOL IsKeyPressed(u32 code) {
  return (GetKeyState(code) >> 15) & 1;
}

int ignoreNextCharEvent = 0;
LRESULT OnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_CHAR:
    if (ignoreNextCharEvent) {
      ignoreNextCharEvent = 0;
    } else if (mode == Insert) {
      if (wParam >= ' ' && wParam <= '}') {
        char chars[1] = {(char)wParam};
        BufferInsertChars(chars, 1, pos);
        UpdateCursor(pos + 1);
      }
    }
    break;
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (mode == Normal) {
      if (wParam == 'I') {
        mode = Insert;
        ignoreNextCharEvent = 1;
      }
      if (wParam == 'Q') {
        PostQuitMessage(0);
        isRunning = 0;
      }
      if (wParam == 'Z') {
        isF = !isF;
        SetFullscreen(win, isF);
      }
      if (wParam == 'X' && IsKeyPressed(VK_SHIFT)) {
        if (pos > 0) {
          BufferRemoveChars(pos - 1, pos - 1);
          UpdateCursor(pos - 1);
        }
      } else if (wParam == 'X') {
        if (pos < (size - 1))
          BufferRemoveChars(pos, pos);
      }

      if (wParam == 'O') {
        i32 offset = GetOffset(pos);
        i32 where = 0;

        if (IsKeyPressed(VK_SHIFT))
          where = Max(FindLineStart(pos) - 1, 0);
        else
          where = FindLineEnd(pos);

        char toInsert[255] = {'\n'};
        for (i32 i = 0; i < offset; i++)
          toInsert[i + 1] = ' ';
        BufferInsertChars(toInsert, offset + 1, where);
        if (where == 0)
          UpdateCursor(where + offset);
        else
          UpdateCursor(where + offset + 1);
        mode = Insert;
        ignoreNextCharEvent = 1;
      }
      if (wParam == 'L')
        MoveRight();
      if (wParam == 'H')
        MoveLeft();
      if (wParam == 'J')
        MoveDown();
      if (wParam == 'K')
        MoveUp();
    }
    if (mode == Insert) {
      if (wParam == VK_ESCAPE)
        mode = Normal;
      if (wParam == VK_RETURN) {
        i32 offset = GetOffset(pos);

        char toInsert[255] = {'\n'};
        for (i32 i = 0; i < offset; i++)
          toInsert[i + 1] = ' ';
        BufferInsertChars(toInsert, offset + 1, pos);
        UpdateCursor(pos + offset + 1);
      }
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    isRunning = 0;
    break;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(handle, &ps);
    EndPaint(handle, &ps);
    break;
  }
  case WM_SIZE:
    Size(lParam);
    break;
  }
  return DefWindowProc(handle, message, wParam, lParam);
}

void OpenWindow() {
  HINSTANCE instance = GetModuleHandle(0);
  WNDCLASSA windowClass = {0};
  windowClass.hInstance = instance;
  windowClass.lpfnWndProc = OnEvent;
  windowClass.lpszClassName = "MyClassName";
  windowClass.style = CS_VREDRAW | CS_HREDRAW;
  windowClass.hCursor = LoadCursor(0, IDC_ARROW);
  windowClass.hbrBackground = CreateSolidBrush(0);

  RegisterClassA(&windowClass);

  HDC screenDc = GetDC(0);

  int screenWidth = GetDeviceCaps(screenDc, HORZRES);
  int screenHeight = GetDeviceCaps(screenDc, VERTRES);

  int width = 1800;
  int height = 1000;
  const char* title = "Hell";

  win = CreateWindowA(windowClass.lpszClassName, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                      screenWidth - width, 0, width, height, 0, 0, instance, 0);

  // if (window->isFullscreen)
  // SetFullscreen(window->windowHandle, 1);

  // dc = GetDC(win);
  ShowWindow(win, SW_SHOW);
  BOOL USE_DARK_MODE = TRUE;
  SUCCEEDED(DwmSetWindowAttribute(win, DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE,
                                  sizeof(USE_DARK_MODE)));
}

HFONT CreateFont(i32 fontSize, const char* name, i32 weight, i32 quiality) {
  int h = -MulDiv(fontSize, GetDeviceCaps(dc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
  return CreateFontA(h, 0, 0, 0,
                     // FW_BOLD,
                     weight,
                     0, // Italic
                     0, // Underline
                     0, // Strikeout
                     DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, quiality,
                     DEFAULT_PITCH, name);
}

i32 Append(char* buffer, const char* str) {
  i32 l = 0;
  while (str[l] != '\0') {
    buffer[l] = str[l];
    l++;
  }
  return l;
}

i32 Append(char* buffer, int v) {
  char temp[20] = {0};
  i32 l = 0;

  if (v == 0) {
    temp[l++] = '0';
  }

  while (v > 0) {
    temp[l++] = '0' + (v % 10);
    v /= 10;
  }

  for (i32 i = l - 1; i >= 0; i--)
    buffer[l - i - 1] = temp[i];

  return l;
}

extern "C" void WinMainCRTStartup() {
  PreventWindowsDPIScaling();
  dc = CreateCompatibleDC(0);
  OpenWindow();

  HFONT f = CreateFont(15, "Consolas", FW_REGULAR, CLEARTYPE_NATURAL_QUALITY);
  SelectObject(dc, f);

  TEXTMETRICA m;
  GetTextMetrics(dc, &m);
  GLYPHSET* set;
  set = (GLYPHSET*)valloc(GetFontUnicodeRanges(dc, nullptr));
  GetFontUnicodeRanges(dc, set);
  i32 r = set->cGlyphsSupported;

  HDC windowDC = GetDC(win);

  const char* path = "progress.txt";
  // const char* path = "m.cpp";
  i64 fileSize = GetMyFileSize(path);
  char* temp = (char*)valloc(fileSize);
  ReadFileInto(path, fileSize, temp);
  capacity = fileSize * 2;
  content = (char*)valloc(capacity);
  size = 0;
  for (i32 i = 0; i < fileSize; i++) {
    if (temp[i] != '\r')
      content[size++] = temp[i];
  }
  vfree(temp);

  i64 frameStart = GetPerfCounter();
  f32 freq = (f32)GetPerfFrequency();
  while (isRunning) {
    MSG msg;

    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    memset(canvas.pixels, 0x00, canvas.width * canvas.height * 4);

    TextColors(0xd0d0d0, 0x000000);

    i32 i = 0;
    i32 x = 20;
    f32 y = 20;

    SIZE s;
    const char* ch = "w";
    GetTextExtentPoint32A(dc, ch, 1, &s);

    while (content[i] != '\0') {
      i32 lineStart = i;
      while (content[i] != '\n' && content[i] != '\0')
        i++;

      TextOutA(dc, x, (i32)y, content + lineStart, i - lineStart);

      if (content[i] == '\0')
        break;
      i++;
      y += (f32)m.tmHeight * 1.1f;
    }

    i32 row = 0;
    i32 rowStartAt = 0;
    for (i32 i = 0; i < pos; i++) {
      if (content[i] == '\n') {
        row++;
        rowStartAt = i + 1;
      }
    }
    i32 col = pos - rowStartAt;

    u32 cursorbg = 0xFFDC32;
    if (mode == Insert)
      cursorbg = 0xFF3269;
    TextColors(0x000000, cursorbg);
    i32 cursorX = x + col * s.cx;
    i32 cursorY = 20 + row * m.tmHeight * 1.1f;
    if (mode == Insert)
      PaintRect(cursorX - 2, cursorY, 2, m.tmHeight, cursorbg);
    else {
      PaintRect(cursorX, cursorY, s.cx, m.tmHeight, cursorbg);

      TextOutA(dc, cursorX, cursorY, content + pos, 1);
    }
    i64 frameEnd = GetPerfCounter();

    char buff[255];
    i32 len = 0;
    len += Append(buff + len, "Frame: ");
    len += Append(buff + len, (frameEnd - frameStart) * 1000.0f / freq);
    len += Append(buff + len, "ms");
    // len += Append(buff + len, " ") + 1;
    // len += Append(buff + len, 142) + 1;

    TextColors(0x888888, 0x000000);
    TextOutA(dc, 20, canvas.height - 30, buff, len);

    StretchDIBits(windowDC, 0, 0, width, height, 0, 0, canvas.width, canvas.height, canvas.pixels,
                  &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
    frameStart = frameEnd;
  }

  ExitProcess(0);
}
