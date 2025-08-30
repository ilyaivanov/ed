#include "win32.c"
#include <malloc.h>

i32 isRunning = 1;

i32 isInitialized = 0;

BITMAPINFO bitmapInfo;
HWND win;
HDC canvasDc;
HDC windowDC;

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

void Draw() {
  memset(canvas.pixels, 0, canvas.width * canvas.height * 4);

  PaintRect(&canvas, canvas.width / 2 - 200 / 2, 20, 200, 200, 0xffffff);

  StretchDIBits(windowDC, 0, 0, canvas.width, canvas.height, 0, 0, canvas.width, canvas.height,
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
    HDC hdc = BeginPaint(handle, &ps);
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

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  //;void WinMainCRTStartup() {
  
  PreventWindowsDPIScaling();
  canvasDc = CreateCompatibleDC(0);
  win = OpenWindow(OnEvent);
  windowDC = GetDC(win);

  char* foo = (char*)malloc(42);

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