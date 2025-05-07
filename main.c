#include "anim.c"
#include "font.c"
#include "math.c"
#include "vim.c"
#include "win32.c"
#include <math.h>

HWND mainWindow;
// u32 colorsBg = 0x0F1419;
u32 colorsBg = 0x121212;
u32 colorsFont = 0xE6E6E6;
u32 colorsFooter = 0x1D1D1D;

u32 colorsCursorNormal = 0xFFDC32;
u32 colorsCursorInsert = 0xFC2837;
u32 colorsCursorVisual = 0x3E9FBE;
u32 colorsCursorLineNormal = 0x3F370E;
u32 colorsCursorLineInsert = 0x3D2425;
u32 colorsCursorLineVisual = 0x202020;

u32 colorsSelection = 0x253340;
u32 colorsScrollbar = 0x222222;

Rect textRect = {0};
Rect footerRect = {0};
Rect screen = {0};

Buffer buffer;
char* filePath = "..\\vim.c";

int isRunning = 1;
int isFullscreen = 0;
Spring offset;

typedef enum Mode { Normal, Insert, Visual } Mode;
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
i32 fontSize = 16;
char* fontName = "Consolas";

typedef struct CursorPos {
  i32 global;

  i32 line;
  i32 lineOffset;
} CursorPos;
CursorPos GetPositionOffset(i32 pos) {
  CursorPos res = {0};
  res.global = -1;

  i32 textSize = buffer.size;
  char* text = buffer.content;

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

CursorPos GetCursorPosition() {
  return GetPositionOffset(buffer.cursor);
}

i32 GetPageHeight() {
  int rows = 1;
  for (i32 i = 0; i < buffer.size; i++) {
    if (buffer.content[i] == '\n')
      rows++;
  }
  return rows * lineHeightPx;
}

void CenterViewOnCursor() {

  CursorPos cursor = GetCursorPosition();
  float cursorPos = cursor.line * lineHeightPx;

  offset.target = cursorPos-(float)textRect.height / 2.0f;
}

void SetCursorPosition(i32 v) {
  buffer.cursor = Clampi32(v, 0, buffer.size - 1);
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
  i32 next = FindLineEnd(&buffer, buffer.cursor);
  if (next != buffer.size) {
    i32 nextNextLine = FindLineEnd(&buffer, next + 1);
    CursorPos cursor = GetCursorPosition();
    SetCursorPosition(MinI32(next + cursor.lineOffset + 1, nextNextLine));
  }
}

void MoveUp() {
  i32 prev = FindLineStart(&buffer, buffer.cursor);
  if (prev != 0) {
    i32 prevPrevLine = FindLineStart(&buffer, prev - 1);
    CursorPos cursor = GetCursorPosition();
    i32 pos = prevPrevLine + cursor.lineOffset;

    SetCursorPosition(MinI32(pos, prev));
  }
}

void RemoveCurrentChar() {
  if (buffer.cursor > 0) {
    RemoveCharAt(&buffer, buffer.cursor - 1);
    buffer.cursor--;
  }
}

void SaveFile() {
  WriteMyFile(filePath, buffer.content, buffer.size);
  isSaved = 1;
}

inline BOOL IsKeyPressed(u32 code) {
  return (GetKeyState(code) >> 15) & 1;
}
void MoveLeft() {
  SetCursorPosition(buffer.cursor - 1);
}
void MoveRight() {
  SetCursorPosition(buffer.cursor + 1);
}

i32 hasMatchedAnyCommand;
i32 IsCommand(char* str) {
  i32 len = strlen(str);
  if (currentCommandLen == len) {
    for (i32 i = 0; i < len; i++) {
      if (currentCommand[i] != str[i])
        return 0;
    }
    hasMatchedAnyCommand = 1;
    return 1;
  }
  return 0;
}

void ClearCommand() {
  visibleCommandLen = currentCommandLen;
  currentCommandLen = 0;
}

void AppendCharIntoCommand(char ch) {
  currentCommand[currentCommandLen++] = ch;
  visibleCommandLen = 0;
  hasMatchedAnyCommand = 0;

  if (IsCommand("j"))
    MoveDown();

  if (IsCommand("k"))
    MoveUp();

  if (IsCommand("h"))
    MoveLeft();

  if (IsCommand("l"))
    MoveRight();

  if (IsCommand("w"))
    SetCursorPosition(JumpWordForward(&buffer));

  if (IsCommand("b"))
    SetCursorPosition(JumpWordBackward(&buffer));

  if (IsCommand("W"))
    SetCursorPosition(JumpWordWithPunctuationForward(&buffer));

  if (IsCommand("B"))
    SetCursorPosition(JumpWordWithPunctuationBackward(&buffer));

  if (IsCommand("gg")) {
    buffer.cursor = 0;
    offset.target = 0;
  }
  if (IsCommand("G")) {
    buffer.cursor = buffer.size - 1;
    offset.target = (GetPageHeight() - textRect.height);
  }
  if (IsCommand("}"))
    SetCursorPosition(JumpParagraphDown(&buffer));
  if (IsCommand("{"))
    SetCursorPosition(JumpParagraphUp(&buffer));

  if (mode == Visual) {
    if (IsCommand("d")) {
      i32 selectionLeft = MinI32(buffer.selectionStart, buffer.cursor);
      i32 selectionRight = MaxI32(buffer.selectionStart, buffer.cursor);
      RemoveChars(&buffer, selectionLeft, selectionRight);
      buffer.cursor = MinI32(buffer.cursor, buffer.selectionStart);
      mode = Normal;
    }
    if (IsCommand("o")) {
      i32 temp = buffer.cursor;
      buffer.cursor = buffer.selectionStart;
      buffer.selectionStart = temp;
    }
    if (IsCommand("y")) {
      i32 selectionLeft = MinI32(buffer.selectionStart, buffer.cursor);
      i32 selectionRight = MaxI32(buffer.selectionStart, buffer.cursor);
      ClipboardCopy(mainWindow, buffer.content + selectionLeft, selectionRight - selectionLeft + 1);
      // mode = Normal;
    }
  } else if (mode == Normal) {
    if (IsCommand("dl") || IsCommand("dd")) {

      int from = FindLineStart(&buffer, buffer.cursor);
      int to = FindLineEnd(&buffer, buffer.cursor);
      RemoveChars(&buffer, from, to);

      SetCursorPosition(buffer.cursor);
    }

    if (IsCommand("dW")) {
      int from = buffer.cursor;
      int to = JumpWordWithPunctuationForward(&buffer) - 1;
      RemoveChars(&buffer, from, to);
    }
    if (IsCommand("dw")) {
      int from = buffer.cursor;
      int to = JumpWordForward(&buffer) - 1;
      RemoveChars(&buffer, from, to);
    }
    if (IsCommand("O")) {
      i32 lineStart = FindLineStart(&buffer, buffer.cursor);
      InsertCharAt(&buffer, lineStart, '\n');
      buffer.cursor = lineStart;
      mode = Insert;
    }
    if (IsCommand("o")) {
      i32 lineEnd = FindLineEnd(&buffer, buffer.cursor);
      InsertCharAt(&buffer, lineEnd, '\n');
      buffer.cursor = lineEnd + 1;
      mode = Insert;
    }
    if (IsCommand("v")) {
      mode = Visual;
      buffer.selectionStart = buffer.cursor;
    }
    if (IsCommand("I")) {
      buffer.cursor = FindLineEnd(&buffer, buffer.cursor);
      mode = Insert;
    }
    if (IsCommand("i")) {
      mode = Insert;
    }
    if (IsCommand("a")) {
      buffer.cursor = MaxI32(buffer.cursor - 1, 0);
      mode = Insert;
    }
    if (IsCommand("A")) {
      buffer.cursor = FindLineStart(&buffer, buffer.cursor);
      while (buffer.cursor < buffer.size && IsWhitespace(buffer.content[buffer.cursor]))
        buffer.cursor++;

      mode = Insert;
    }
    if (IsCommand("C")) {
      i32 start = FindLineStart(&buffer, buffer.cursor);
      i32 end = FindLineEnd(&buffer, buffer.cursor) - 1;
      if (start != end) {
        RemoveChars(&buffer, buffer.cursor, end);
      }
      mode = Insert;
    }
    if (IsCommand("D")) {
      i32 start = FindLineStart(&buffer, buffer.cursor);
      i32 end = FindLineEnd(&buffer, buffer.cursor) - 1;
      if (start != end) {
        RemoveChars(&buffer, buffer.cursor, end);
        buffer.cursor -= 1;
      }
    }

    if (IsCommand("yy") || IsCommand("yl")) {
      i32 start = FindLineStart(&buffer, buffer.cursor);
      i32 end = FindLineEnd(&buffer, buffer.cursor);
      ClipboardCopy(mainWindow, buffer.content + start, end - start + 1);
    }
    if (IsCommand("p")) {
      i32 size = 0;
      char* textFromClipboard = ClipboardPaste(mainWindow, &size);

      InsertChars(&buffer, textFromClipboard, size, buffer.cursor + 1);
      buffer.cursor = buffer.cursor + 1 + size;

      if (textFromClipboard)
        VirtualFreeMemory(textFromClipboard);
    }

    if (IsCommand("P")) {
      i32 size = 0;
      char* textFromClipboard = ClipboardPaste(mainWindow, &size);

      InsertChars(&buffer, textFromClipboard, size, buffer.cursor);
      buffer.cursor = buffer.cursor + size;

      if (textFromClipboard)
        VirtualFreeMemory(textFromClipboard);
    }
    if (IsCommand("zz")) 
      CenterViewOnCursor();

    if (IsCommand("tt")) {
      PostQuitMessage(0);
      isRunning = 0;
    }
  }

  if (hasMatchedAnyCommand)
    ClearCommand();
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
    if (ch >= ' ' && ch <= MAX_CHAR_CODE) {
      if (mode == Normal || mode == Visual)
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
    } else if (mode == Visual) {
      if (wParam == VK_ESCAPE)
        mode = Normal;
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
      u32* pixel = &canvas.pixels[j * canvas.width + i];
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
  char* label = filePath;
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
    CopyMonochromeTextureRectTo(&canvas, &footerRect, &font.textures[currentCommand[i]], commandX,
                                y, color);
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

    PaintRect(textRect.x + textRect.width - scrollbarWidth, textRect.y + scrollY, scrollbarWidth,
              (i32)scrollbarHeight, colorsScrollbar);
  }
}

void DrawSelection() {
  u32 bg = colorsSelection;
  i32 padding = 10;
  i32 x = padding;
  i32 y = padding - (i32)offset.current;
  if (mode == Visual) {
    u32 selectionLeft = MinI32(buffer.selectionStart, buffer.cursor);
    u32 selectionRight = MaxI32(buffer.selectionStart, buffer.cursor) + 1;

    CursorPos startPos = GetPositionOffset(selectionLeft);
    CursorPos endPos = GetPositionOffset(selectionRight);

    i32 len = GetLineLength(&buffer, startPos.line);
    i32 maxLen = len - startPos.lineOffset;
    i32 firstLineLen = MinI32(selectionRight - selectionLeft, maxLen);

    PaintRect(x + startPos.lineOffset * font.charWidth, y + startPos.line * lineHeightPx,
              firstLineLen * font.charWidth, lineHeightPx, bg);

    if (startPos.line != endPos.line) {
      for (i32 l = startPos.line + 1; l < endPos.line; l++) {
        PaintRect(x, y + l * lineHeightPx, GetLineLength(&buffer, l) * font.charWidth, lineHeightPx,
                  bg);
      }

      PaintRect(x, y + endPos.line * lineHeightPx, endPos.lineOffset * font.charWidth, lineHeightPx,
                bg);
    }
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

  u32 cursorColor = mode == Normal   ? colorsCursorNormal
                    : mode == Visual ? colorsCursorVisual
                                     : colorsCursorInsert;

  CursorPos cursor = GetCursorPosition();
  i32 cursorX = x + font.charWidth * cursor.lineOffset;
  i32 cursorY = y + lineHeightPx * cursor.line;

  u32 lineColor = mode == Normal   ? colorsCursorLineNormal
                  : mode == Visual ? colorsCursorLineVisual
                                   : colorsCursorLineInsert;
  // cursor line background
  PaintRect(0, cursorY, textRect.width, lineHeightPx, lineColor);

  // selection
  DrawSelection();

  // cursor
  PaintRect(cursorX, cursorY, font.charWidth, lineHeightPx, cursorColor);
  i32 charShift = (lineHeightPx - font.charHeight) / 2;

  for (i32 i = 0; i < buffer.size; i++) {
    i32 charY = y + charShift;
    char ch = buffer.content[i];
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
  StretchDIBits(dc, 0, 0, screen.width, screen.height, 0, 0, screen.width, screen.height,
                canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

inline i64 EllapsedMs(i64 start) {
  return (i64)((f32)(GetPerfCounter() - start) * 1000.0f / (f32)GetPerfFrequency());
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  (void)hPrevInstance;
  (void)lpCmdLine;
  (void)nShowCmd;

  PreventWindowsDPIScaling();

  mainWindow = OpenWindow(OnEvent, colorsBg, "Editor");

  dc = GetDC(mainWindow);
  buffer = ReadFileIntoDoubledSizedBuffer(filePath);

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
