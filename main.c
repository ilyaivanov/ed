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

u32 colorsCursorUnfocused = 0x666666;
u32 colorsCursorLineUnfocused = 0x333333;

u32 colorsCursorNormal = 0xFFDC32;
u32 colorsCursorInsert = 0xFC2837;
u32 colorsCursorVisual = 0x3E9FBE;
u32 colorsCursorLineNormal = 0x3F370E;
u32 colorsCursorLineInsert = 0x3D2425;
u32 colorsCursorLineVisual = 0x202020;

u32 colorsSelection = 0x253340;
u32 colorsScrollbar = 0x888888;

float scrollbarWidth = 10;

Rect leftRect;
Spring leftOffset;

Rect middleRect;
Spring middleOffset;

Rect rightRect;
Spring rightOffset;

Rect footerRect = {0};
i32 horizPadding = 20;
i32 vertPadding = 10;
i32 footerPadding = 2;
Rect screen = {0};

Buffer leftBuffer;
char* leftFilePath = "..\\misc\\tasks.txt";

Buffer middleBuffer;
char* middleFilePath = "..\\main.c";

Buffer rightBuffer;
char* rightFilePath = "..\\vim.c";

typedef enum EdFile { Left, Middle, Right } EdFile;

Buffer* selectedBuffer;
Spring* selectedOffset;
Rect* selectedRect;
EdFile selectedFile;

void SelectFile(EdFile file) {
  selectedFile = file;
  if (file == Left) {
    selectedBuffer = &leftBuffer;
    selectedOffset = &leftOffset;
    selectedRect = &leftRect;
  }
  if (file == Right) {
    selectedBuffer = &rightBuffer;
    selectedOffset = &rightOffset;
    selectedRect = &rightRect;
  }
  if (file == Middle) {
    selectedBuffer = &middleBuffer;
    selectedOffset = &middleOffset;
    selectedRect = &middleRect;
  }
}
int isRunning = 1;
int isFullscreen = 1;

typedef enum Mode { Normal, Insert, Visual } Mode;
f32 appTimeMs = 0;
Mode mode = Normal;

BITMAPINFO bitmapInfo;
MyBitmap canvas;
HDC dc;
char currentCommand[512];
i32 currentCommandLen;
i32 visibleCommandLen;
FontData font;

i32 lineHeightPx;
f32 lineHeight = 1.1;
i32 fontSize = 15;
char* fontName = "Consolas";

typedef struct CursorPos {
  i32 global;

  i32 line;
  i32 lineOffset;
} CursorPos;

CursorPos GetPositionOffset(Buffer* buffer, i32 pos) {
  CursorPos res = {0};
  res.global = -1;

  i32 textSize = buffer->size;
  char* text = buffer->content;

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

CursorPos GetCursorPosition(Buffer* buffer) {
  return GetPositionOffset(buffer, buffer->cursor);
}

i32 GetPageHeight(Buffer* buffer) {
  int rows = 1;
  for (i32 i = 0; i < buffer->size; i++) {
    if (buffer->content[i] == '\n')
      rows++;
  }
  return rows * lineHeightPx;
}

void CenterViewOnCursor() {

  CursorPos cursor = GetCursorPosition(selectedBuffer);
  float cursorPos = cursor.line * lineHeightPx;

  selectedOffset->target = cursorPos - (float)selectedRect->height / 2.0f;
}

void SetCursorPosition(i32 v) {
  selectedBuffer->cursor = Clampi32(v, 0, selectedBuffer->size - 1);
  CursorPos cursor = GetCursorPosition(selectedBuffer);

  float lineToLookAhead = 5.0f * lineHeightPx;
  float cursorPos = cursor.line * lineHeightPx;
  float maxScroll = GetPageHeight(selectedBuffer) - selectedRect->height;
  if ((selectedOffset->target + selectedRect->height - lineToLookAhead) < cursorPos)
    selectedOffset->target = Clamp(cursorPos - (float)selectedRect->height / 2.0f, 0, maxScroll);
  else if ((selectedOffset->target + lineToLookAhead) > cursorPos)
    selectedOffset->target = Clamp(cursorPos - (float)selectedRect->height / 2.0f, 0, maxScroll);
}

void MoveDown() {
  i32 next = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
  if (next != selectedBuffer->size) {
    i32 nextNextLine = FindLineEnd(selectedBuffer, next + 1);
    CursorPos cursor = GetCursorPosition(selectedBuffer);
    SetCursorPosition(MinI32(next + cursor.lineOffset + 1, nextNextLine));
  }
}

void MoveUp() {
  i32 prev = FindLineStart(selectedBuffer, selectedBuffer->cursor);
  if (prev != 0) {
    i32 prevPrevLine = FindLineStart(selectedBuffer, prev - 1);
    CursorPos cursor = GetCursorPosition(selectedBuffer);
    i32 pos = prevPrevLine + cursor.lineOffset;

    SetCursorPosition(MinI32(pos, prev));
  }
}

void RemoveCurrentChar() {
  if (selectedBuffer->cursor > 0) {
    RemoveChar(selectedBuffer, selectedBuffer->cursor - 1);
    selectedBuffer->cursor--;
  }
}

void SaveSelectedFile() {
  WriteMyFile(selectedBuffer->filePath, selectedBuffer->content, selectedBuffer->size);
  selectedBuffer->isSaved = 1;
}

inline BOOL IsKeyPressed(u32 code) {
  return (GetKeyState(code) >> 15) & 1;
}
void MoveLeft() {
  SetCursorPosition(selectedBuffer->cursor - 1);
}
void MoveRight() {
  SetCursorPosition(selectedBuffer->cursor + 1);
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
    SetCursorPosition(JumpWordForward(selectedBuffer));

  if (IsCommand("b"))
    SetCursorPosition(JumpWordBackward(selectedBuffer));

  if (IsCommand("W"))
    SetCursorPosition(JumpWordWithPunctuationForward(selectedBuffer));

  if (IsCommand("B"))
    SetCursorPosition(JumpWordWithPunctuationBackward(selectedBuffer));

  if (IsCommand("gg")) {
    selectedBuffer->cursor = 0;
    selectedOffset->target = 0;
  }
  if (IsCommand("G")) {
    selectedBuffer->cursor = selectedBuffer->size - 1;
    selectedOffset->target = (GetPageHeight(selectedBuffer) - selectedRect->height);
  }
  if (IsCommand("}"))
    SetCursorPosition(JumpParagraphDown(selectedBuffer));
  if (IsCommand("{"))
    SetCursorPosition(JumpParagraphUp(selectedBuffer));

  if (mode == Visual) {
    if (IsCommand("d")) {
      i32 selectionLeft = MinI32(selectedBuffer->selectionStart, selectedBuffer->cursor);
      i32 selectionRight = MaxI32(selectedBuffer->selectionStart, selectedBuffer->cursor);
      RemoveChars(selectedBuffer, selectionLeft, selectionRight);
      selectedBuffer->cursor = MinI32(selectedBuffer->cursor, selectedBuffer->selectionStart);
      mode = Normal;
    }
    if (IsCommand("o")) {
      i32 temp = selectedBuffer->cursor;
      selectedBuffer->cursor = selectedBuffer->selectionStart;
      selectedBuffer->selectionStart = temp;
    }
    if (IsCommand("y")) {
      i32 selectionLeft = MinI32(selectedBuffer->selectionStart, selectedBuffer->cursor);
      i32 selectionRight = MaxI32(selectedBuffer->selectionStart, selectedBuffer->cursor);
      ClipboardCopy(mainWindow, selectedBuffer->content + selectionLeft,
                    selectionRight - selectionLeft + 1);
      // mode = Normal;
    }
  } else if (mode == Normal) {
    if (IsCommand("dl") || IsCommand("dd")) {

      int from = FindLineStart(selectedBuffer, selectedBuffer->cursor);
      int to = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
      RemoveChars(selectedBuffer, from, to);

      SetCursorPosition(selectedBuffer->cursor);
    }

    if (IsCommand("dW")) {
      int from = selectedBuffer->cursor;
      int to = JumpWordWithPunctuationForward(selectedBuffer) - 1;
      RemoveChars(selectedBuffer, from, to);
    }
    if (IsCommand("dw")) {
      int from = selectedBuffer->cursor;
      int lineEnd = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
      int to = MinI32(JumpWordForward(selectedBuffer) - 1, lineEnd - 1);
      RemoveChars(selectedBuffer, from, to);
    }
    if (IsCommand("O")) {
      i32 lineStart = FindLineStart(selectedBuffer, selectedBuffer->cursor);
      InsertCharAt(selectedBuffer, lineStart, '\n');
      selectedBuffer->cursor = lineStart;
      mode = Insert;
    }
    if (IsCommand("o")) {
      i32 lineEnd = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
      InsertCharAt(selectedBuffer, lineEnd, '\n');
      selectedBuffer->cursor = lineEnd + 1;
      mode = Insert;
    }
    if (IsCommand("v")) {
      mode = Visual;
      selectedBuffer->selectionStart = selectedBuffer->cursor;
    }
    if (IsCommand("I")) {
      selectedBuffer->cursor = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
      mode = Insert;
    }
    if (IsCommand("i")) {
      mode = Insert;
    }
    if (IsCommand("a")) {
      selectedBuffer->cursor = MaxI32(selectedBuffer->cursor - 1, 0);
      mode = Insert;
    }
    if (IsCommand("A")) {
      selectedBuffer->cursor = FindLineStart(selectedBuffer, selectedBuffer->cursor);
      while (selectedBuffer->cursor < selectedBuffer->size &&
             IsWhitespace(selectedBuffer->content[selectedBuffer->cursor]))
        selectedBuffer->cursor++;

      mode = Insert;
    }
    if (IsCommand("C")) {
      i32 start = FindLineStart(selectedBuffer, selectedBuffer->cursor);
      i32 end = FindLineEnd(selectedBuffer, selectedBuffer->cursor) - 1;
      if (start != end) {
        RemoveChars(selectedBuffer, selectedBuffer->cursor, end);
      }
      mode = Insert;
    }
    if (IsCommand("D")) {
      i32 start = FindLineStart(selectedBuffer, selectedBuffer->cursor);
      i32 end = FindLineEnd(selectedBuffer, selectedBuffer->cursor) - 1;
      if (start != end) {
        RemoveChars(selectedBuffer, selectedBuffer->cursor, end);
        selectedBuffer->cursor -= 1;
      }
    }

    if (IsCommand("x")) {
      if (selectedBuffer->cursor < selectedBuffer->size - 1)
        RemoveChar(selectedBuffer, selectedBuffer->cursor);
    }
    if (IsCommand("yy") || IsCommand("yl")) {
      i32 start = FindLineStart(selectedBuffer, selectedBuffer->cursor);
      i32 end = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
      ClipboardCopy(mainWindow, selectedBuffer->content + start, end - start + 1);
    }
    if (IsCommand("p")) {
      i32 size = 0;
      char* textFromClipboard = ClipboardPaste(mainWindow, &size);

      InsertChars(selectedBuffer, textFromClipboard, size, selectedBuffer->cursor + 1);
      selectedBuffer->cursor = selectedBuffer->cursor + 1 + size;

      if (textFromClipboard)
        VirtualFreeMemory(textFromClipboard);
    }

    if (IsCommand("P")) {
      i32 size = 0;
      char* textFromClipboard = ClipboardPaste(mainWindow, &size);

      InsertChars(selectedBuffer, textFromClipboard, size, selectedBuffer->cursor);
      selectedBuffer->cursor = selectedBuffer->cursor + size;

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
          InsertCharAtCursor(selectedBuffer, ch);
        selectedBuffer->isSaved = 0;
      }
    }
    break;
  case WM_SYSKEYDOWN:
  case WM_KEYDOWN:
    if (mode == Insert) {
      if (wParam == VK_ESCAPE)
        mode = Normal;
      if (wParam == VK_RETURN)
        InsertCharAtCursor(selectedBuffer, '\n');
      else if (wParam == 'B' && IsKeyPressed(VK_CONTROL))
        RemoveCurrentChar();
      else if (wParam == VK_BACK)
        RemoveCurrentChar();
    } else if (mode == Normal) {
      if (wParam == VK_RETURN)
        InsertCharAtCursor(selectedBuffer, '\n');
      else if (wParam == VK_BACK)
        RemoveCurrentChar();
      if (wParam == VK_ESCAPE)
        ClearCommand();
      if (wParam == '1' && IsKeyPressed(VK_MENU))
        SelectFile(Left);
      if (wParam == '2' && IsKeyPressed(VK_MENU))
        SelectFile(Middle);
      if (wParam == '3' && IsKeyPressed(VK_MENU))
        SelectFile(Right);
      if (wParam == 'S' && IsKeyPressed(VK_CONTROL))
        SaveSelectedFile();
      else if (wParam == VK_F11) {
        isFullscreen = !isFullscreen;
        SetFullscreen(window, isFullscreen);
      } else if (IsKeyPressed(VK_CONTROL) && wParam == 'D') {
        selectedOffset->target += (float)selectedRect->height / 2.0f;
      } else if (IsKeyPressed(VK_CONTROL) && wParam == 'U') {
        selectedOffset->target -= (float)selectedRect->height / 2.0f;
      }
    } else if (mode == Visual) {
      if (wParam == VK_ESCAPE)
        mode = Normal;
    }
    break;
  case WM_SYSCOMMAND:
    if (wParam == SC_KEYMENU)
      return 0;

    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    isRunning = 0;
    break;
  case WM_SIZE:
    canvas.width = screen.width = LOWORD(lParam);
    canvas.height = screen.height = HIWORD(lParam);

    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biWidth = screen.width;
    // makes rows go down, instead of going up by default
    bitmapInfo.bmiHeader.biHeight = -screen.height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    if (canvas.pixels)
      VirtualFreeMemory(canvas.pixels);

    canvas.pixels = VirtualAllocateMemory(canvas.width * canvas.height * 4);
    dc = GetDC(window);

    int footerHeight = font.charHeight + footerPadding * 2;
    footerRect.width = screen.width;
    footerRect.height = footerHeight;
    footerRect.y = screen.height - footerRect.height;

    leftRect.width = 80 * font.charWidth;
    leftRect.height = canvas.height - footerRect.height;

    i32 codeWidth = (screen.width - leftRect.width) / 2;
    middleRect.x = leftRect.width;
    middleRect.width = canvas.width / 3;
    middleRect.height = canvas.height - footerRect.height;

    rightRect.x = middleRect.x + middleRect.width;
    rightRect.width = canvas.width - (leftRect.width + middleRect.width);
    rightRect.height = canvas.height - footerRect.height;
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

void RectFill(Rect r, u32 color) {
  PaintRect(r.x, r.y, r.width, r.height, color);
}

void RectFillRightBorder(Rect r, i32 width, u32 color) {
  PaintRect(r.x + r.width - width / 2, r.y, width, r.height, color);
}

void DrawFooter() {
  RectFill(footerRect, colorsFooter);

  char* label = selectedBuffer->filePath;
  int len = strlen(label);
  int x = footerPadding;
  int y = footerRect.y;
  while (*label) {
    CopyMonochromeTextureRectTo(&canvas, &footerRect, &font.textures[*label], x, y, 0xffffff);
    x += font.charWidth;
    label++;
  }
  if (!selectedBuffer->isSaved)
    CopyMonochromeTextureRectTo(&canvas, &footerRect, &font.textures['*'], x, y, 0xffffff);

  int charsToShow = currentCommandLen > 0 ? currentCommandLen : visibleCommandLen;
  int commandX = footerRect.width / 2;
  for (int i = 0; i < charsToShow; i++) {
    u32 color = currentCommandLen > 0 ? 0xffffff : 0xbbbbbb;
    CopyMonochromeTextureRectTo(&canvas, &footerRect, &font.textures[currentCommand[i]], commandX,
                                y, color);
    commandX += font.charWidth;
  }
}

void DrawScrollBar(Rect rect, Buffer* buffer, Spring* offset) {
  f32 pageHeight = (f32)GetPageHeight(buffer);
  if (rect.height < pageHeight) {
    f32 height = (f32)rect.height;
    f32 scrollOffset = (f32)offset->current;

    f32 scrollbarHeight = (height * height) / pageHeight;

    f32 maxOffset = pageHeight - height;
    f32 maxScrollY = height - scrollbarHeight;
    f32 scrollY = lerp(0, maxScrollY, scrollOffset / maxOffset);

    PaintRect(rect.x + rect.width - scrollbarWidth, rect.y + scrollY, scrollbarWidth,
              (i32)scrollbarHeight, colorsScrollbar);
  }
}

void DrawSelection(Buffer* buffer, Rect rect, Spring* offset) {
  u32 bg = colorsSelection;
  i32 x = horizPadding + rect.x;
  i32 y = vertPadding + rect.y - (i32)offset->current;
  if (mode == Visual) {
    u32 selectionLeft = MinI32(selectedBuffer->selectionStart, selectedBuffer->cursor);
    u32 selectionRight = MaxI32(selectedBuffer->selectionStart, selectedBuffer->cursor) + 1;

    CursorPos startPos = GetPositionOffset(buffer, selectionLeft);
    CursorPos endPos = GetPositionOffset(buffer, selectionRight);

    i32 len = GetLineLength(selectedBuffer, startPos.line);
    i32 maxLen = len - startPos.lineOffset;
    i32 firstLineLen = MinI32(selectionRight - selectionLeft, maxLen);

    PaintRect(x + startPos.lineOffset * font.charWidth, y + startPos.line * lineHeightPx,
              firstLineLen * font.charWidth, lineHeightPx, bg);

    if (startPos.line != endPos.line) {
      for (i32 l = startPos.line + 1; l < endPos.line; l++) {
        PaintRect(x, y + l * lineHeightPx, GetLineLength(selectedBuffer, l) * font.charWidth,
                  lineHeightPx, bg);
      }

      PaintRect(x, y + endPos.line * lineHeightPx, endPos.lineOffset * font.charWidth, lineHeightPx,
                bg);
    }
  }
}

void DrawArea(Rect rect, Buffer* buffer, Spring* offset, EdFile file) {
  i32 startX = rect.x + horizPadding;
  i32 startY = vertPadding - (i32)offset->current;
  i32 x = startX;
  i32 y = rect.y + startY;

  u32 cursorColor = mode == Normal   ? colorsCursorNormal
                    : mode == Visual ? colorsCursorVisual
                                     : colorsCursorInsert;

  CursorPos cursor = GetCursorPosition(buffer);
  i32 cursorX = x + font.charWidth * cursor.lineOffset;
  i32 cursorY = y + lineHeightPx * cursor.line;

  u32 lineColor = mode == Normal   ? colorsCursorLineNormal
                  : mode == Visual ? colorsCursorLineVisual
                                   : colorsCursorLineInsert;

  if (file != selectedFile) {
    cursorColor = colorsCursorUnfocused;
    lineColor = colorsCursorLineUnfocused;
  }

  if (!buffer->isSaved)
    PaintRect(rect.x + rect.width - 30 - scrollbarWidth, rect.y, 20, 10, 0x882222);

  // cursor line background
  PaintRect(rect.x, cursorY, rect.width, lineHeightPx, lineColor);

  // selection
  if (selectedFile == file)
    DrawSelection(buffer, rect, offset);

  // cursor
  PaintRect(cursorX, cursorY, font.charWidth, lineHeightPx, cursorColor);
  i32 charShift = (lineHeightPx - font.charHeight) / 2;

  for (i32 i = 0; i < buffer->size; i++) {
    i32 charY = y + charShift;
    char ch = buffer->content[i];
    if (ch == '\n') {
      x = startX;
      y += lineHeightPx;
    } else if (ch < MAX_CHAR_CODE && ch >= ' ') {
      u32 charColor = i == buffer->cursor ? colorsBg : colorsFont;
      CopyMonochromeTextureRectTo(&canvas, &rect, &font.textures[ch], x, charY, charColor);
      x += font.charWidth;
    } else {
      PaintRect(x, charY, font.charWidth, lineHeightPx, 0xff0000);
      x += font.charWidth;
    }
  }

  DrawScrollBar(rect, buffer, offset);
}

void Draw() {
  for (i32 i = 0; i < canvas.width * canvas.height; i++)
    canvas.pixels[i] = colorsBg;

  u32 borderColor = 0x222222;
  RectFillRightBorder(leftRect, 4, borderColor);
  RectFillRightBorder(middleRect, 4, borderColor);
  RectFillRightBorder(rightRect, 4, borderColor);

  DrawArea(leftRect, &leftBuffer, &leftOffset, Left);
  DrawArea(middleRect, &middleBuffer, &middleOffset, Middle);
  DrawArea(rightRect, &rightBuffer, &rightOffset, Right);

  DrawFooter();

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

  Arena fontArena = CreateArena(500 * 1024);
  InitFont(&font, fontName, fontSize, &fontArena);

  mainWindow = OpenWindow(OnEvent, colorsBg, "Editor");

  if (isFullscreen)
    SetFullscreen(mainWindow, 1);

  dc = GetDC(mainWindow);
  leftBuffer = ReadFileIntoDoubledSizedBuffer(leftFilePath);
  leftBuffer.filePath = leftFilePath;
  leftBuffer.isSaved = 1;

  middleBuffer = ReadFileIntoDoubledSizedBuffer(middleFilePath);
  middleBuffer.filePath = middleFilePath;
  middleBuffer.isSaved = 1;

  rightBuffer = ReadFileIntoDoubledSizedBuffer(rightFilePath);
  rightBuffer.filePath = rightFilePath;
  rightBuffer.isSaved = 1;

  SelectFile(Left);
  InitAnimations();
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
    float deltaSec = deltaMs / 1000.0f;
    UpdateSpring(&leftOffset, deltaSec);
    UpdateSpring(&middleOffset, deltaSec);
    UpdateSpring(&rightOffset, deltaSec);
    appTimeMs += deltaMs;
    startCounter = endCounter;
  }
  return 0;
}
