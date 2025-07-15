#include "math.cpp"
#include "win32.cpp"
#include <stdio.h>

Window window;
u32 color = 0xcccccc;
u32 bg = 0x000000;
u32 bgEven = 0x444444;

TEXTMETRIC textMetric;

HBITMAP canvasBitmap;
HDC currentDc;

i32 hasInit = 0;
i32 currentBuffer = 0;
enum Mode { Normal, Insert };

Mode mode = Normal;
HFONT font;

i32 cursorPos = 0;
i32 desiredOffset = 0;

u32 ToWinColor(u32 color) {
  return ((color & 0xff0000) >> 16) | (color & 0x00ff00) | ((color & 0x0000ff) << 16);
}

void TextColors(u32 foreground, u32 background) {
  SetBkColor(currentDc, ToWinColor(background));
  SetTextColor(currentDc, ToWinColor(foreground));
}

void Init() {
  hasInit = 1;

  i32 fontSize = 14;

  int h = -MulDiv(fontSize, GetDeviceCaps(window.dc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
  font = CreateFontA(h, 0, 0, 0,
                     // FW_BOLD,
                     FW_REGULAR, // Weight
                     0,          // Italic
                     0,          // Underline
                     0,          // Strikeout
                     DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH, "Consolas");

  SelectObject(currentDc, font);
  GetTextMetricsA(currentDc, &textMetric);
}

void Size(LPARAM lParam) {
  window.width = LOWORD(lParam);
  window.height = HIWORD(lParam);
  InitBitmapInfo(&window.bitmapInfo, window.width, window.height);
  if (canvasBitmap)
    DeleteObject(canvasBitmap);

  canvasBitmap = CreateDIBSection(currentDc, &window.bitmapInfo, DIB_RGB_COLORS,
                                  (void**)&window.canvas.pixels, 0, 0);
  window.canvas.width = window.width;
  window.canvas.height = window.height;
  window.canvas.bytesPerPixel = 4;

  SelectObject(currentDc, canvasBitmap);
}

LRESULT OnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_SYSCOMMAND: {
    // handle alt
    if (wParam == SC_KEYMENU) {
      return 0;
    }
    break;
  }
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (wParam == VK_F11) {
      window.isFullscreen = !window.isFullscreen;
      SetFullscreen(window.windowHandle, window.isFullscreen);
    }
    if (wParam == 'Q') {
      PostQuitMessage(0);
      window.isTerminated = 1;
    }
    if (mode == Normal) {

      if (wParam == 'L')
        cursorPos++;
      if (wParam == 'H')
        cursorPos--;
      if (wParam == 'I')
        mode = Insert;
    } else if (mode == Insert) {
      if (wParam == VK_ESCAPE)
        mode = Normal;
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    window.isTerminated = 1;
    break;
  case WM_SIZE:
    Size(lParam);
    break;
  }
  return DefWindowProc(handle, message, wParam, lParam);
}

void PaintRect(v2 pos, f32 width, f32 height, u32 color) {
  int startX = round(pos.x);
  int startY = round(pos.y);
  int endX = round(pos.x + width);
  int endY = round(pos.y + height);

  startX = Max(startX, 0);
  startY = Max(startY, 0);

  endX = Min(endX, window.canvas.width);
  endY = Min(endY, window.canvas.height);

  for (i32 y = startY; y < endY; y++) {
    for (i32 x = startX; x < endX; x++) {
      window.canvas.pixels[x + y * window.canvas.width] = color;
    }
  }
}

f32 measurements[100];
i32 currentMeasurement;
void PrintFrameStats() {
  TextColors(0xffffff, bg);
  f32 total = 0;
  i32 count = 0;
  for (i32 i = 0; i < ArrayLength(measurements); i++) {
    if (measurements[i] != 0.0f) {
      total += measurements[i];
      count++;
    }
  }
  f32 average = 0;
  if (count != 0)
    average = total / (f32)count;

  char foo[255];
  i32 l = sprintf(foo, "Frame: %0.1fms", average * 1000.0f);
  TextOutA(currentDc, 10, window.height - textMetric.tmHeight - 6, &foo[0], l);
}

f32 lineHeight = 1.2f;

void UpdateAndDraw(f32 deltaSec) {
  const char* ch = "First line\nSecond line\nFourth line";

  i32 lineStart = 0;
  i32 len = strlen(ch);
  i32 padding = 20;
  i32 x = padding;
  i32 y = padding;

  i32 line = 0;
  while (lineStart < len) {
    i32 lineEnd = lineStart;
    while (lineEnd < len && ch[lineEnd] != '\n')
      lineEnd++;

    SetTextColor(currentDc, color);
    SetBkColor(currentDc, bg);

    TextOutA(currentDc, x, y, ch + lineStart, lineEnd - lineStart);
    lineStart = lineEnd + 1;
    lineEnd += 2;
    line++;
    y += textMetric.tmHeight * lineHeight;
  }
  i32 cursorLine = 0;
  i32 cursorOffset = 0;
  for (i32 i = 0; i < cursorPos; i++) {
    if (ch[i] == '\n') {
      cursorLine++;
      cursorOffset = 0;
    } else {
      cursorOffset++;
    }
  }
  f32 cursorX = padding + (f32)cursorOffset * (f32)textMetric.tmAveCharWidth;
  f32 cursorY = padding + cursorLine * textMetric.tmHeight * lineHeight;
  u32 cursorColor = mode == Normal ? 0xFFDC32 : 0xff2222;
  u32 colorUnderCursor = 0x000000;
  PaintRect(V2{cursorX, cursorY}, textMetric.tmAveCharWidth, textMetric.tmHeight, cursorColor);
  TextColors(colorUnderCursor, cursorColor);
  TextOutA(currentDc, cursorX, cursorY, ch + cursorPos, 1);

  PrintFrameStats();
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  PreventWindowsDPIScaling();

  window = {.width = 1400,
            .height = 2100,
            .bg = 0x222222,
            .title = "Editor",
            .isFullscreen = 0,
            .onEvent = OnEvent};

  currentDc = CreateCompatibleDC(0);
  OpenWindow(&window);

  i64 frameStart = GetPerfCounter();
  i64 appStart = frameStart;
  i64 frameEnd = frameStart;
  f32 freq = (f32)GetPerfFrequency();

  Init();

  while (!window.isTerminated) {
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    memset(window.canvas.pixels, 0x00, window.canvas.width * window.canvas.height * 4);

    f32 deltaSec = f32(frameEnd - frameStart) / freq;
    UpdateAndDraw(deltaSec);

    measurements[currentMeasurement++] = deltaSec;
    if (currentMeasurement >= ArrayLength(measurements))
      currentMeasurement = 0;

    frameStart = frameEnd;

    frameEnd = GetPerfCounter();

    // Sleep(15);
    BitBlt(window.dc, 0, 0, window.width, window.height, currentDc, 0, 0, SRCCOPY);

    // Sleep(8);
    // Sleep(2);
    // Sleep(100);
  }
  return 0;
}

// 12.0ms for textout (9.3 without bitblit)
// 5.0ms for printstr (2.3 without bitblit)
// 2.7ms for bitblit
