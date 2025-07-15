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
char* file;
i32 fileSize;
i32 fileCapacity;
i32 isSpace;
i32 ignoreNextCharEvent; // this is used to ignore char event which comes immeidatelly after
f32 buildTimeMs;
// entering insert mode

char* output;
i32 outputLen;

u32 ToWinColor(u32 color) {
  return ((color & 0xff0000) >> 16) | (color & 0x00ff00) | ((color & 0x0000ff) << 16);
}

void TextColors(u32 foreground, u32 background) {
  SetBkColor(currentDc, ToWinColor(background));
  SetTextColor(currentDc, ToWinColor(foreground));
}

void ReadFileIntoDoubledSizedBuffer(const char* path) {
  i64 fileSize2 = GetMyFileSize(path);
  char* filetemp = (char*)VirtualAllocateMemory(fileSize2);
  ReadFileInto(path, fileSize2, filetemp);

  fileCapacity = fileSize2 * 2;
  fileSize = 0;
  file = (char*)VirtualAllocateMemory(fileCapacity);
  for (i32 i = 0; i < fileSize2; i++) {
    if (filetemp[i] != '\r')
      file[fileSize++] = filetemp[i];
  }
  VirtualFreeMemory(filetemp);
}

void Init() {
  hasInit = 1;
  output = (char*)VirtualAllocateMemory(MB(1));

  ReadFileIntoDoubledSizedBuffer("./console.cpp");

  i32 fontSize = 14;

  int h = -MulDiv(fontSize, GetDeviceCaps(window.dc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
  font = CreateFontA(h, 0, 0, 0,
                     // FW_BOLD,
                     FW_REGULAR, // Weight
                     0,          // Italic
                     0,          // Underline
                     0,          // Strikeout
                     DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH, "Consolas");

  SelectObject(currentDc, font);
  GetTextMetricsA(currentDc, &textMetric);
}

i32 FindLineEnd(i32 pos) {
  while (pos < fileSize && file[pos] != '\n')
    pos++;

  return pos;
}

i32 FindLineStart(i32 pos) {
  while (pos > 0 && file[pos - 1] != '\n')
    pos--;

  return pos;
}

void MoveDown() {
  cursorPos = FindLineEnd(cursorPos) + 1;
  if (cursorPos >= fileSize)
    cursorPos = fileSize;
}
void MoveUp() {
  cursorPos = FindLineStart(FindLineStart(cursorPos) - 1);
  if (cursorPos < 0)
    cursorPos = 0;
  cursorPos += desiredOffset;
}
void MoveLeft() {
  if (cursorPos > 0) {
    cursorPos--;
  }
}
void MoveRight() {
  if (cursorPos < (fileSize - 1)) {
    cursorPos++;
  }
}

void Run() {
  i64 start = GetPerfCounter();
  WriteMyFile((char*)"console.cpp", file, fileSize);
  char* s = (char*)"cmd /c clang console.cpp";
  RunCommand(s, output, &outputLen);

  // clang produce no output on case of success
  if (outputLen == 0) {

    char* s2 = (char*)"cmd /c a.exe";
    RunCommand(s2, output, &outputLen);
  }
  i64 end = GetPerfCounter();
  buildTimeMs = f32(end - start) * 1000.0f / (f32)GetPerfFrequency();
}

void InsertCharAtCursor(char ch) {
  if (fileSize >= fileCapacity) {
    i32 newCapacity = fileCapacity * 2;
    char* newMemory = (char*)VirtualAllocateMemory(newCapacity);
    fileCapacity = newCapacity;
    memmove(newMemory, file, fileSize);
    VirtualFreeMemory(file);
    file = newMemory;
  }
  memmove(file + cursorPos + 1, file + cursorPos, fileSize - cursorPos);
  file[cursorPos] = ch;
  cursorPos++;
  fileSize++;
}

void RemoveChar() {
  if (cursorPos > 0) {
    memmove(file + cursorPos - 1, file + cursorPos, fileSize - cursorPos);
    cursorPos--;
    fileSize--;
  }
}
void RemoveCharFromRight() {
  if (cursorPos < (fileSize - 1)) {
    memmove(file + cursorPos, file + cursorPos + 1, fileSize - cursorPos - 1);
    fileSize--;
  }
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
  case WM_CHAR: {
    if (ignoreNextCharEvent)
      ignoreNextCharEvent = 0;
    else if (mode == Insert && wParam >= ' ' && wParam <= 126)
      InsertCharAtCursor(wParam);
    break;
  }
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (wParam == VK_F11) {
      window.isFullscreen = !window.isFullscreen;
      SetFullscreen(window.windowHandle, window.isFullscreen);
    }
    if (mode == Normal) {
      if (wParam == 'Q') {
        PostQuitMessage(0);
        window.isTerminated = 1;
      }
      if (wParam == 'L')
        MoveRight();
      if (wParam == 'H')
        MoveLeft();
      if (wParam == 'J')
        MoveDown();
      if (wParam == 'K')
        MoveUp();
      if (wParam == 'I') {
        mode = Insert;
        ignoreNextCharEvent = 1;
      }
      if (wParam == 'R' && IsKeyPressed(VK_MENU))
        Run();

      if (wParam == VK_SPACE)
        isSpace = 1;
      if (wParam == VK_BACK)
        RemoveChar();
      if (wParam == 'D')
        RemoveCharFromRight();

      if (wParam == VK_RETURN)
        InsertCharAtCursor('\n');
    } else if (mode == Insert) {
      if (wParam == VK_ESCAPE)
        mode = Normal;
      if (wParam == VK_BACK) {
        RemoveChar();
      }
      if (wParam == VK_RETURN)
        InsertCharAtCursor('\n');
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    window.isTerminated = 1;
    break;
  case WM_KEYUP:
    if (wParam == VK_SPACE)
      isSpace = 0;
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
  i32 l = sprintf(foo, "Frame: %0.1fms Build: %0.1fms", average * 1000.0f, buildTimeMs);
  TextOutA(currentDc, 10, window.height - textMetric.tmHeight - 6, &foo[0], l);
}

f32 lineHeight = 1.1f;

void PrintParagraph(i32 x, i32 y, char* ch, i32 len) {
  i32 line = 0;
  i32 lineStart = 0;
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
}

void UpdateAndDraw(f32 deltaSec) {
  i32 lineStart = 0;
  i32 len = fileSize;
  i32 padding = 20;
  i32 x = padding;
  i32 y = padding;

  i32 line = 0;
  char* ch = file;

  SIZE size;
  const char* c = "w";
  GetTextExtentPoint32W(currentDc, (LPCWSTR)c, 1, &size);
  i32 charWidth = size.cx;

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
  f32 cursorX = padding + (f32)cursorOffset * (f32)charWidth;
  f32 cursorY = padding + cursorLine * textMetric.tmHeight * lineHeight;
  u32 cursorColor = mode == Normal ? 0xFFDC32 : 0xff2222;
  u32 colorUnderCursor = 0x000000;

  if (mode == Normal) {
    PaintRect(V2{cursorX, cursorY}, charWidth, textMetric.tmHeight, cursorColor);

    TextColors(colorUnderCursor, cursorColor);
    TextOutA(currentDc, cursorX, cursorY, ch + cursorPos, 1);
  } else if (mode == Insert) {
    f32 barWidth = 2.0f;
    PaintRect(V2{cursorX - barWidth / 2, cursorY}, barWidth, textMetric.tmHeight, cursorColor);
  }

  PrintFrameStats();
  if (outputLen > 0)
    PrintParagraph(20, 400, output, outputLen);
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

    BitBlt(window.dc, 0, 0, window.width, window.height, currentDc, 0, 0, SRCCOPY);
  }
  return 0;
}

// 12.0ms for textout (9.3 without bitblit)
// 5.0ms for printstr (2.3 without bitblit)
// 2.7ms for bitblit
