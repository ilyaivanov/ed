#include "win32.cpp"
#include <malloc.h>

i32 isRunning = 1;

i32 isInitialized = 0;

BITMAPINFO bitmapInfo;
HWND win;
HDC canvasDc;
HDC windowDc;
HFONT font;

HBITMAP canvasBitmap;

MyBitmap canvas;

void Size(LPARAM lParam) {
  canvas.width = LOWORD(lParam);
  canvas.height = HIWORD(lParam);
  InitBitmapInfo(&bitmapInfo, canvas.width, canvas.height);
  if (canvasBitmap)
    DeleteObject(canvasBitmap);

  canvasBitmap =
      CreateDIBSection(canvasDc, &bitmapInfo, DIB_RGB_COLORS, (void**)&canvas.pixels, 0, 0);
  canvas.bytesPerPixel = 4;

  SelectObject(canvasDc, canvasBitmap);
}

HFONT CreateFontf(i32 fontSize, const char* name, i32 weight, i32 quiality) {
  int h = -MulDiv(fontSize, GetDeviceCaps(canvasDc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
  return CreateFontA(h, 0, 0, 0,
                     // FW_BOLD,
                     weight,
                     0, // Italic
                     0, // Underline
                     0, // Strikeout
                     DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, quiality,
                     DEFAULT_PITCH, name);
}

void Draw() {
  memset(canvas.pixels, 0, canvas.width * canvas.height * 4);

  u32 b = 0x000000;
  u32 f = 0xffffff;
  SetBkColor(canvasDc, ToWinColor(b));
  SetTextColor(canvasDc, ToWinColor(f));

  const char* str = "public static void main(char* args)";

  SelectObject(canvasDc, font);
  TextOutA(canvasDc, 20, 20, str, strlen(str));

  StretchDIBits(windowDc, 0, 0, canvas.width, canvas.height, 0, 0, canvas.width, canvas.height,
                canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT OnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_KEYDOWN:
    if (wParam == 'Q') {
      PostQuitMessage(0);
      isRunning = 0;
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    isRunning = 0;
    break;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    BeginPaint(handle, &ps);
    EndPaint(handle, &ps);
  } break;
  case WM_SIZE:
    Size(lParam);
    if (isInitialized)
      Draw();
    break;
  }
  return DefWindowProc(handle, message, wParam, lParam);
}

int WinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
            [[maybe_unused]] LPSTR lpCmdLine, [[maybe_unused]] int nShowCmd) {
  
  SetProcessDPIAware();
  
  canvasDc = CreateCompatibleDC(0);
  win = OpenWindow(OnEvent);
  windowDc = GetDC(win);

  i32 size = 15;
  font = CreateFontf(size, "Consolas", FW_REGULAR, CLEARTYPE_NATURAL_QUALITY);

  isInitialized = 1;
  while (isRunning) {
    MSG msg;

    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    Draw();
  }

  ExitProcess(0);
}