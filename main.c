#include "anim.c"
#include "font.c"
#include "math.c"
#include "string.c"
#include "vim.c"
#include "win32.c"
#include <math.h>

// u32 colorsBg = 0x0F1419;
u32 colorsBg = 0x121212;
u32 colorsFont = 0xE6E6E6;
u32 colorsFooter = 0x1D1D1D;

u32 colorsCursorNormal = 0xFFDC32;
u32 colorsCursorInsert = 0xFC2837;
u32 colorsCursorLineNormal = 0x3F370E;
u32 colorsCursorLineInsert = 0x3D2425;

u32 colorsScrollbar = 0x222222;

Rect textRect = {0};
Rect footerRect = {0};
Rect screen = {0};

Buffer buffer;
char *filePath = "..\\vim.c";

int isRunning = 1;
int isFullscreen = 0;
Spring offset;

typedef enum Mode { Normal, Insert } Mode;
f32 appTimeMs = 0;
Mode mode = Normal;

BITMAPINFO bitmapInfo;
MyBitmap canvas;
HDC dc;
i32 isSaved = 1;
char currentCommand[512];
i32 currentCommandLen;
i32 visibleCommandLen;
FontData font;

i32 lineHeightPx;
f32 lineHeight = 1.1;
i32 fontSize = 14;
char *fontName = "Consolas";

typedef struct CursorPos {
  i32 global;

  i32 line;
  i32 lineOffset;
} CursorPos;

CursorPos GetCursorPosition() {
  CursorPos res = {0};
  res.global = -1;

  i32 pos = buffer.cursor;
  i32 textSize = buffer.file.size;
  char *text = buffer.file.content;

  if (pos >= 0 && pos <= textSize) {
    res.global = pos;
    res.line = 0;

    i32 lineStartedAt = 0;
    for (i32 i = 0; i < pos; i++) {
      if (text[i] == '\n') {
        res.line++;
        lineStartedAt = i + 1;
      }
    }

    res.lineOffset = pos - lineStartedAt;
  }
  return res;
}

i32 GetPageHeight() {
  int rows = 2;
  for (i32 i = 0; i < buffer.file.size; i++) {
    if (buffer.file.content[i] == '\n')
      rows++;
  }
  return rows * lineHeightPx;
}

void SetCursorPosition(i32 v) {
  buffer.cursor = Clampi32(v, 0, buffer.file.size);
  CursorPos cursor = GetCursorPosition();

  float lineToLookAhead = 5.0f * lineHeightPx;
  float cursorPos = cursor.line * lineHeightPx;
  float maxScroll = GetPageHeight() - textRect.height;
  if ((offset.target + textRect.height - lineToLookAhead) < cursorPos)
    offset.target = Clamp(cursorPos - (float)textRect.height / 2.0f, 0, maxScroll);
  else if ((offset.target + lineToLookAhead) > cursorPos)
    offset.target = Clamp(cursorPos - (float)textRect.height / 2.0f, 0, maxScroll);
}

void MoveDown() {
  i32 next = FindLineEnd(buffer.cursor);
  if (next != buffer.file.size) {
    i32 nextNextLine = FindLineEnd(next + 1);
    CursorPos cursor = GetCursorPosition();
    SetCursorPosition(MinI32(next + cursor.lineOffset + 1, nextNextLine));
  }
}

void MoveUp() {
  i32 prev = FindLineStart(buffer.cursor);
  if (prev != 0) {
    i32 prevPrevLine = FindLineStart(prev - 1);
    CursorPos cursor = GetCursorPosition();
    i32 pos = prevPrevLine + cursor.lineOffset;

    SetCursorPosition(MinI32(pos, prev));
  }
}

void RemoveCurrentChar() {
  if (buffer.cursor > 0) {
    RemoveCharAt(buffer.cursor - 1);
    buffer.cursor--;
  }
}

void SaveFile() {
  WriteMyFile(filePath, buffer.file.content, buffer.file.size);
  isSaved = 1;
}

inline BOOL IsKeyPressed(u32 code) {
  return (GetKeyState(code) >> 15) & 1;
}
void MoveLeft() {
  buffer.cursor = MaxI32(buffer.cursor - 1, 0);
}
void MoveRight() {
  buffer.cursor = MinI32(buffer.cursor + 1, buffer.file.size);
}

void AppendCharIntoCommand(char ch) {
  currentCommand[currentCommandLen++] = ch;
  visibleCommandLen = 0;
  if (currentCommand[0] == 'd') {
    if (currentCommandLen == 2 && (currentCommand[1] == 'l' || currentCommand[1] == 'd')) {
      int from = FindLineStart(buffer.cursor);
      int to = FindLineEnd(buffer.cursor);
      RemoveChars(&buffer.file, from, to);
      buffer.cursor = MinI32(buffer.cursor, buffer.file.size);
      visibleCommandLen = currentCommandLen;
      currentCommandLen = 0;
    }
    if (currentCommandLen == 2 && currentCommand[1] == 'W') {
      int from = buffer.cursor;
      int to = JumpWordWithPunctuationForward(&buffer) - 1;
      RemoveChars(&buffer.file, from, to);
      visibleCommandLen = currentCommandLen;
      currentCommandLen = 0;
    }
    if (currentCommandLen == 2 && currentCommand[1] == 'w') {
      int from = buffer.cursor;
      int to = JumpWordForward(&buffer) - 1;
      RemoveChars(&buffer.file, from, to);
      visibleCommandLen = currentCommandLen;
      currentCommandLen = 0;
    }
  }
  if (currentCommand[0] == 'A') {
    buffer.cursor = FindLineStart(buffer.cursor);
    while (buffer.cursor < buffer.file.size && IsWhitespace(buffer.file.content[buffer.cursor]))
      buffer.cursor++;

    mode = Insert;
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'O') {
    i32 lineStart = FindLineStart(buffer.cursor);
    InsertCharAt(&buffer, lineStart, '\n');
    buffer.cursor = lineStart;
    mode = Insert;
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'o') {
    i32 lineEnd = FindLineEnd(buffer.cursor);
    InsertCharAt(&buffer, lineEnd, '\n');
    buffer.cursor = lineEnd + 1;
    mode = Insert;
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'I') {
    buffer.cursor = FindLineEnd(buffer.cursor);
    mode = Insert;
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'i') {
    mode = Insert;
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'a') {
    buffer.cursor = MaxI32(buffer.cursor - 1, 0);
    mode = Insert;
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'j') {
    MoveDown();
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'k') {
    MoveUp();
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'h') {
    MoveLeft();
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'l') {
    MoveRight();
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'w') {
    SetCursorPosition(JumpWordForward(&buffer));
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'b') {
    SetCursorPosition(JumpWordBackward(&buffer));
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'W') {
    SetCursorPosition(JumpWordWithPunctuationForward(&buffer));
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'B') {
    SetCursorPosition(JumpWordWithPunctuationBackward(&buffer));
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommandLen == 2 && currentCommand[0] == 'g' && currentCommand[1] == 'g') {
    buffer.cursor = 0;
    offset.target = 0;
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }
  if (currentCommand[0] == 'G') {
    buffer.cursor = buffer.file.size;
    offset.target = (GetPageHeight() - textRect.height);
    visibleCommandLen = currentCommandLen;
    currentCommandLen = 0;
  }

  if (currentCommandLen == 2 && currentCommand[0] == 't' && currentCommand[1] == 't') {
    PostQuitMessage(0);
    isRunning = 0;
  }
}

void ClearCommand() {
  visibleCommandLen = currentCommandLen;
  currentCommandLen = 0;
}

LRESULT OnEvent(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {

  case WM_PAINT:
    PAINTSTRUCT paint = {0};
    BeginPaint(window, &paint);
    EndPaint(window, &paint);
    break;
  case WM_CHAR:
    char ch = (char)wParam;
    if (ch >= 'A' && ch <= 'z' || ch == ' ') {
      if (mode == Normal)
        AppendCharIntoCommand(ch);
      else if (mode == Insert) {
        if (ch < MAX_CHAR_CODE && ch >= ' ')
          InsertCharAtCursor(&buffer, ch);
        isSaved = 0;
      }
    }
    break;
  case WM_KEYDOWN:
    if (mode == Insert) {
      if (wParam == VK_ESCAPE)
        mode = Normal;
      if (wParam == VK_RETURN)
        InsertCharAtCursor(&buffer, '\n');
      else if (wParam == 'B' && IsKeyPressed(VK_CONTROL))
        RemoveCurrentChar();
    } else if (mode == Normal) {
      if (wParam == VK_RETURN)
        InsertCharAtCursor(&buffer, '\n');
      if (wParam == VK_ESCAPE)
        ClearCommand();
      else if (wParam == VK_F11) {
        isFullscreen = !isFullscreen;
        SetFullscreen(window, isFullscreen);
      } else if (IsKeyPressed(VK_CONTROL) && wParam == 'D') {
        offset.target += (float)textRect.height / 2.0f;
      } else if (IsKeyPressed(VK_CONTROL) && wParam == 'U') {
        offset.target -= (float)textRect.height / 2.0f;
      }
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    isRunning = 0;
    break;
  case WM_SIZE:
    screen.width = LOWORD(lParam);
    screen.height = HIWORD(lParam);

    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biWidth = screen.width;
    bitmapInfo.bmiHeader.biHeight = -screen.height;
    // makes rows go down, instead of
    // going up by default
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    canvas.width = screen.width;
    canvas.height = screen.height;
    if (canvas.pixels)
      VirtualFreeMemory(canvas.pixels);

    canvas.pixels = VirtualAllocateMemory(canvas.width * canvas.height * 4);
    dc = GetDC(window);
  }
  return DefWindowProc(window, message, wParam, lParam);
}

void PaintRectAlpha(i32 x, i32 y, i32 width, i32 height, u32 color, f32 a) {
  i32 x0 = x < 0 ? 0 : x;
  i32 y0 = y < 0 ? 0 : y;
  i32 x1 = (x + width) > canvas.width ? canvas.width : (x + width);
  i32 y1 = (y + height) > canvas.height ? canvas.height : (y + height);

  for (i32 j = y0; j < y1; j++) {
    for (i32 i = x0; i < x1; i++) {
      u32 *pixel = &canvas.pixels[j * canvas.width + i];
      *pixel = AlphaBlendColors(*pixel, color, a);
    }
  }
}

void PaintRect(i32 x, i32 y, i32 width, i32 height, u32 color) {
  i32 x0 = x < 0 ? 0 : x;
  i32 y0 = y < 0 ? 0 : y;
  i32 x1 = (x + width) > canvas.width ? canvas.width : (x + width);
  i32 y1 = (y + height) > canvas.height ? canvas.height : (y + height);

  for (i32 j = y0; j < y1; j++) {
    for (i32 i = x0; i < x1; i++) {
      canvas.pixels[j * canvas.width + i] = color;
    }
  }
}
i32 footerPadding = 2;
void DrawFooter() {
  char *label = filePath;
  int len = strlen(label);
  PaintRect(footerRect.x, footerRect.y, footerRect.width, footerRect.height, colorsFooter);
  int x = footerPadding;
  int y = footerRect.y;
  while (*label) {
    CopyMonochromeTextureRectTo(&canvas, &footerRect, &font.textures[*label], x, y, 0xffffff);
    x += font.charWidth;
    label++;
  }
  if (!isSaved)
    CopyMonochromeTextureRectTo(&canvas, &footerRect, &font.textures['*'], x, y, 0xffffff);

  int charsToShow = currentCommandLen > 0 ? currentCommandLen : visibleCommandLen;
  int commandX = footerRect.width / 2 - (charsToShow * font.charWidth) / 2;
  for (int i = 0; i < charsToShow; i++) {
    u32 color = currentCommandLen > 0 ? 0xffffff : 0xbbbbbb;
    CopyMonochromeTextureRectTo(&canvas, &footerRect, &font.textures[currentCommand[i]], commandX, y, color);
    commandX += font.charWidth;
  }
}

void DrawScrollBar() {
  f32 pageHeight = (f32)GetPageHeight();
  float scrollbarWidth = 10;
  if (textRect.height < pageHeight) {
    f32 height = (f32)textRect.height;
    f32 scrollOffset = (f32)offset.current;

    f32 scrollbarHeight = (height * height) / pageHeight;

    f32 maxOffset = pageHeight - height;
    f32 maxScrollY = height - scrollbarHeight;
    f32 scrollY = lerp(0, maxScrollY, scrollOffset / maxOffset);

    PaintRect(textRect.x + textRect.width - scrollbarWidth, textRect.y + scrollY, scrollbarWidth, (i32)scrollbarHeight,
              colorsScrollbar);
  }
}
void Draw() {
  for (i32 i = 0; i < canvas.width * canvas.height; i++)
    canvas.pixels[i] = colorsBg;

  footerRect.width = screen.width;
  int footerHeight = font.charHeight + footerPadding * 2;
  footerRect.height = footerHeight;
  footerRect.y = screen.height - footerRect.height;
  textRect.width = screen.width;
  textRect.height = screen.height - footerRect.height;

  i32 padding = 10;
  i32 startY = padding - (i32)offset.current;
  i32 x = padding;
  i32 y = startY;

  u32 cursorColor = mode == Normal ? colorsCursorNormal : colorsCursorInsert;
  CursorPos cursor = GetCursorPosition();
  i32 cursorX = x + font.charWidth * cursor.lineOffset;
  i32 cursorY = y + lineHeightPx * cursor.line;

  u32 lineColor = mode == Normal ? colorsCursorLineNormal : colorsCursorLineInsert;
  PaintRect(0, cursorY, textRect.width, lineHeightPx, lineColor);
  PaintRect(cursorX, cursorY, font.charWidth, lineHeightPx, cursorColor);
  i32 charShift = (lineHeightPx - font.charHeight) / 2;

  for (i32 i = 0; i < buffer.file.size; i++) {
    i32 charY = y + charShift;
    char ch = buffer.file.content[i];
    if (ch == '\n') {
      x = padding;
      y += lineHeightPx;
    } else if (ch < MAX_CHAR_CODE && ch >= ' ') {
      u32 charColor = i == buffer.cursor ? colorsBg : colorsFont;
      CopyMonochromeTextureRectTo(&canvas, &textRect, &font.textures[ch], x, charY, charColor);
      x += font.charWidth;
    } else {
      PaintRect(x, charY, font.charWidth, lineHeightPx, 0xff0000);
      x += font.charWidth;
    }
  }

  DrawFooter();
  DrawScrollBar();
  StretchDIBits(dc, 0, 0, screen.width, screen.height, 0, 0, screen.width, screen.height, canvas.pixels, &bitmapInfo,
                DIB_RGB_COLORS, SRCCOPY);
}

inline i64 EllapsedMs(i64 start) {
  return (i64)((f32)(GetPerfCounter() - start) * 1000.0f / (f32)GetPerfFrequency());
}
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  (void)hPrevInstance;
  (void)lpCmdLine;
  (void)nShowCmd;

  PreventWindowsDPIScaling();

  HWND window = OpenWindow(OnEvent, colorsBg, "Editor");

  dc = GetDC(window);
  buffer.file = ReadFileIntoDoubledSizedBuffer(filePath);
  currentFile = &buffer.file;

  InitAnimations();
  Arena fontArena = CreateArena(500 * 1024);
  InitFont(&font, fontName, fontSize, &fontArena);
  lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);
  i64 startCounter = GetPerfCounter();
  while (isRunning) {
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    Draw();
    // Sleep(10);
    i64 endCounter = GetPerfCounter();
    float deltaMs = ((f32)(endCounter - startCounter) * 1000.0f / (f32)GetPerfFrequency());
    UpdateSpring(&offset, deltaMs / 1000.0f);
    appTimeMs += deltaMs;
    startCounter = endCounter;
  }
  return 0;
}
