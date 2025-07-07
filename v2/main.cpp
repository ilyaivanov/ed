#include "font2.cpp"
#include "math.cpp"
#include "win32.cpp"
#include <stdio.h>

FontData font;
FontData fileFont;
FontData linesFont;

Window window;
MyBitmap texture;
f32 lineHeight = 1.2;
i32 fontSize = 14;
u32 color = 0xffffff;
u32 bg = 0x008800;

i32 fileSize;
char* file;

const char* path = "./sample/main.cpp";
const char* notespath = "./sample/notes.txt";
const char* subnotespath = "./sample/subfolder/subnotes.txt";

i32 fileSize2;
char* file2;

i32 fileSize3;
char* file3;

typedef struct Cursor {
  i32 pos;
  i32 desiredOffset;
} Cursor;

Cursor cursor;

i32 collapsedLines[255] = {2};

void SetCursorPos(i32 pos) {
  cursor.pos = Clampi(pos, 0, fileSize - 1);
}

i32 FindLineEnd(i32 pos) {
  while (pos < fileSize && file[pos] != '\n')
    pos++;

  return pos;
}

i32 FindLineStart(i32 pos) {
  while (pos >= 0 && file[pos] != '\n')
    pos--;

  return pos;
}

char* ReadTextFile(const char* path, i32* size) {
  i32 fileNativeSize = GetMyFileSize(path);
  char* nativeFile = (char*)VirtualAllocateMemory(fileNativeSize);
  ReadFileInto(path, fileNativeSize, nativeFile);

  char* res = (char*)VirtualAllocateMemory(fileNativeSize * 2);
  int fileSizeAfter = 0;
  for (i32 i = 0; i < fileNativeSize; i++) {
    if (nativeFile[i] != '\r')
      res[fileSizeAfter++] = nativeFile[i];
  }

  *size = fileSizeAfter;
  VirtualFreeMemory(nativeFile);
  return res;
}

LRESULT OnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_KEYDOWN:
    if (wParam == VK_F11) {
      window.isFullscreen = !window.isFullscreen;
      SetFullscreen(window.windowHandle, window.isFullscreen);
    }
    if (wParam == 'Q') {
      PostQuitMessage(0);
      window.isTerminated = 1;
    }
    if (wParam == 'J') {
      i32 end = FindLineEnd(cursor.pos);
      SetCursorPos(end + 1);
    }
    if (wParam == 'K') {
      i32 start = FindLineStart(cursor.pos - 1);
      SetCursorPos(start);
    }
    if (wParam == 'L') {
      i32 next = cursor.pos + 1;
      SetCursorPos(next);
    }
    if (wParam == 'H') {
      i32 next = cursor.pos - 1;
      SetCursorPos(next);
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    window.isTerminated = 1;
    break;
  case WM_SIZE:
    OnSize(&window, lParam);
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

i32 PrintTitle(i32 x, i32 y, const char* label) {
  i32 startY = y;
  f32 lineHeightOffset = (f32)fileFont.charHeight * (lineHeight - 1.0f) / 2.0f;
  y += lineHeightOffset;
  while (*label) {

    char ch = *label;
    if (ch >= ' ' && ch <= MAX_CHAR_CODE) {
      CopyMonochromeTextureRectTo(&window.canvas, &fileFont.textures[ch], x, i32(y));
    }
    x += fileFont.charWidth;
    label++;
  }
  return fileFont.charHeight * lineHeight;
}

void PrintNumber(i32 x, i32 y, i32 val) {
  x -= linesFont.charWidth;
  char formatted[255];
  i32 len = sprintf(formatted, "%d", val);
  for (i32 i = len - 1; i >= 0; i--) {
    CopyMonochromeTextureRectTo(&window.canvas, &linesFont.textures[formatted[i]], x, y);
    x -= linesFont.charWidth;
  }
}

i32 IsCollapsed(i32 line) {
  for (i32 i = 0; i < ArrayLength(collapsedLines); i++) {
    if (collapsedLines[i] == line)
      return 1;
  }
  return 0;
}
i32 GetOffsetAt(i32 pos) {
  i32 res = 0;
  while (file[pos] == ' ' && pos < fileSize) {
    res++;
    pos++;
  }
  return res;
}

i32 PrintParagraph(i32 x, i32 y, i32 size, const char* file) {
  f32 lineHeightOffset = (f32)font.charHeight * (lineHeight - 1.0f) / 2.0f;
  i32 startY = y;
  i32 startX = x;
  i32 line = 0;
  i32 manualAdjustmentForSmallFont = 4;
  
  for (i32 i = 0; i < size; i++) {
    char ch = file[i];

    if (ch >= ' ' && ch <= MAX_CHAR_CODE) {
      CopyMonochromeTextureRectTo(&window.canvas, &font.textures[ch], x, i32(y + lineHeightOffset));
      x += font.charWidth;
    }
    if (ch == '\n') {
      x = startX;
      PrintNumber(x - linesFont.charWidth, y + manualAdjustmentForSmallFont, line + 1);

      y += i32((font.charHeight * lineHeight) + 0.5f);

        line++;
      if (IsCollapsed(line)) {
        // line++;
        // x += 20;
        // i32 currentOffset = GetOffsetAt(i);
        // for(i32 r = 0; r < 2; r++){
        //   i32 end = FindLineEnd(i);
        //   i = end + 1;
        //   line ++;
        // }
        // i += 20;
      } else {
      }
    }
  }
  return y - startY;
}

void PrintCursor(i32 x, i32 y) {
  i32 startX = x;
  for (i32 i = 0; i < fileSize; i++) {
    char ch = file[i];
    if (i == cursor.pos) {
      PaintRect(V2{0, (f32)y}, window.canvas.width, font.charHeight * lineHeight, 0x002200);
      PaintRect(V2{(f32)x, (f32)y}, font.charWidth, font.charHeight * lineHeight, bg);
    }

    if (ch >= ' ' && ch <= MAX_CHAR_CODE) {
      x += font.charWidth;
    }
    if (ch == '\n') {
      y += i32((font.charHeight * lineHeight) + 0.5f);
      x = startX;
    }
  }
}

void UpdateAndDraw(f32 deltaSec) {
  i32 y = 20;
  i32 x = 20;

  y += PrintTitle(x, y, "main.cpp");

  f32 lineHeightOffset = (f32)font.charHeight * (lineHeight - 1.0f) / 2.0f;

  x += font.charWidth * 2;
  PrintCursor(x, y);
  y += PrintParagraph(x, y, fileSize, file);

  x -= font.charWidth * 2;
  y += PrintTitle(x, y, "subfolder");
  x += font.charWidth * 2;
  y += PrintTitle(x, y, "subnotes.txt");

  x += font.charWidth * 2;
  y += PrintParagraph(x, y, fileSize3, file3);

  x -= font.charWidth * 2;
  x -= font.charWidth * 2;
  y += PrintTitle(x, y, "notes.txt");

  x += font.charWidth * 2;
  y += PrintParagraph(x, y, fileSize2, file2);
}

typedef struct Item {
  char* title;
  Item* children;
  i32 childrenCount;
} Item;

void Draw2(f32 deltaSec) {}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  PreventWindowsDPIScaling();

  window = {.width = 1400,
            .height = 1300,
            .bg = 0x222222,
            .title = "Tiny Game 1",
            .isFullscreen = 0,
            .onEvent = OnEvent};

  OpenWindow(&window);

  file = ReadTextFile(path, &fileSize);
  file2 = ReadTextFile(notespath, &fileSize2);
  file3 = ReadTextFile(subnotespath, &fileSize3);
  i64 frameStart = GetPerfCounter();
  i64 appStart = frameStart;
  i64 frameEnd = frameStart;
  f32 freq = (f32)GetPerfFrequency();

  Arena arena = CreateArena(10 * 1024 * 1024);
  font.color = 0xeeeeee;
  InitFont(&font, "Consolas", fontSize, &arena);

  fileFont.weight = FW_BOLD;
  fileFont.color = 0xffffff;
  InitFont(&fileFont, "Consolas", fontSize + 2, &arena);

  linesFont.color = 0x888888;
  InitFont(&linesFont, "Consolas", 12, &arena);

  while (!window.isTerminated) {
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    memset(window.canvas.pixels, 0x00, window.canvas.width * window.canvas.height * 4);

    f32 deltaSec = f32(frameEnd - frameStart) / freq;
    UpdateAndDraw(deltaSec);
    f32 ellapsedSec = f32(frameStart - appStart) / freq;
    frameStart = frameEnd;

    frameEnd = GetPerfCounter();

    PaintWindow(&window);
  }
  return 0;
}
