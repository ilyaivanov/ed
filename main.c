
#include "font.c"
#include "win32.c"
#include <math.h>

// u32 colorsBg = 0x0F1419;
u32 colorsBg = 0x050505;
u32 colorsFont = 0xE6E1CF;

u32 colorsCursorNormal = 0x880000;
u32 colorsCursorInsert = 0x008800;
u32 colorsCursorLineNormal = 0x101510;
u32 colorsCursorLineInsert = 0x151010;

int isRunning = 1;
u32 height;
u32 width;

typedef struct StringBuffer {
  char *content;
  i32 size;
  i32 capacity;
} StringBuffer;

typedef enum Mode { Normal, Insert } Mode;
f32 appTimeMs = 0;
char *filePath = "..\\misc\\sample.txt";
Mode mode = Normal;
StringBuffer file;
BITMAPINFO bitmapInfo;
MyBitmap canvas;
HDC dc;
i32 recX = 1;
i32 recY = 1;
HFONT font;
i32 cursorPos = 0;
i32 isJustEnteredInsert;
i32 isSaved = 1;
char currentCommand[512];
i32 currentCommandLen;
i32 visibleCommandLen;

// Math
inline i32 MinI32(i32 v1, i32 v2) {
  return v1 < v2 ? v1 : v2;
}
inline i32 MaxI32(i32 v1, i32 v2) {
  return v1 > v2 ? v1 : v2;
}
i32 Clampi32(i32 val, i32 min, i32 max) {
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}

// Text
inline void MoveBytesLeft(char *ptr, int length) {
  for (int i = 0; i < length - 1; i++) {
    ptr[i] = ptr[i + 1];
  }
}

inline void PlaceLineEnd(StringBuffer *buffer) {
  if (buffer->content)
    *(buffer->content + buffer->size) = '\0';
}

void RemoveCharAt(StringBuffer *buffer, i32 at) {
  MoveBytesLeft(buffer->content + at, buffer->size - at);
  buffer->size--;
  PlaceLineEnd(buffer);
}

void InsertCharAtCursor(char ch) {
  i32 textSize = file.size;
  i32 pos = cursorPos;
  char *text = file.content;
  memmove(text + pos + 1, text + pos, textSize - pos);

  text[pos] = ch;

  file.size++;
  cursorPos++;
}

i32 FindLineStart(i32 pos) {
  while (pos > 0 && file.content[pos - 1] != '\n')
    pos--;

  return pos;
}

i32 FindLineEnd(i32 pos) {
  i32 textSize = file.size;
  char *text = file.content;
  while (pos < textSize && text[pos] != '\n')
    pos++;

  return pos;
}

typedef struct CursorPos {
  i32 global;
  i32 line;
  i32 lineOffset;
} CursorPos;

CursorPos GetCursorPosition() {
  CursorPos res = {0};
  res.global = -1;

  i32 pos = cursorPos;
  i32 textSize = file.size;
  char *text = file.content;

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
void SetCursorPosition(i32 v) {
  cursorPos = Clampi32(v, 0, file.size);
}

void MoveDown() {
  i32 next = FindLineEnd(cursorPos);
  if (next != file.size) {
    i32 nextNextLine = FindLineEnd(next + 1);
    CursorPos cursor = GetCursorPosition();
    SetCursorPosition(MinI32(next + cursor.lineOffset + 1, nextNextLine));
  }
}

void MoveUp() {
  i32 prev = FindLineStart(cursorPos);
  if (prev != 0) {
    i32 prevPrevLine = FindLineStart(prev - 1);
    CursorPos cursor = GetCursorPosition();
    i32 pos = prevPrevLine + cursor.lineOffset;

    SetCursorPosition(MinI32(pos, prev));
  }
}

char whitespaceChars[] = {' ', '\n'};
u32 IsWhitespace(char ch) {
  for (i32 i = 0; i < ArrayLength(whitespaceChars); i++) {
    if (whitespaceChars[i] == ch)
      return 1;
  }

  return 0;
}

void JumpWordForward() {
  i32 pos = cursorPos;

  char *text = file.content;
  while (pos < file.size && !IsWhitespace(text[pos]))
    pos++;

  while (pos < file.size && IsWhitespace(text[pos]))
    pos++;

  SetCursorPosition(pos);
}

void JumpWordBackward() {
  i32 pos = cursorPos == 0 ? 0 : cursorPos - 1;

  char *text = file.content;
  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  while (pos > 0 && !IsWhitespace(text[pos - 1]))
    pos--;

  SetCursorPosition(pos);
}

void RemoveCurrentChar() {
  if (cursorPos > 0) {
    RemoveCharAt(&file, cursorPos - 1);
    cursorPos--;
  }
}

void SaveFile() {
  WriteMyFile(filePath, file.content, file.size);
  isSaved = 1;
}

inline BOOL IsKeyPressed(u32 code) {
  return (GetKeyState(code) >> 15) & 1;
}

void TryPerformCommand() {
  // if(command
}

void AppendCharIntoCommand(char ch) {

  currentCommand[currentCommandLen++] = ch;
  visibleCommandLen = 0;
  if (currentCommand[0] == 'j') {
    MoveDown();
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
    if (ch >= 'A' && ch < 'z') {
      if (isJustEnteredInsert)
        isJustEnteredInsert = 0;
      else if (mode == Normal)
        AppendCharIntoCommand(ch);
      else if (mode == Insert) {
        if (ch < MAX_CHAR_CODE && ch >= ' ')
          InsertCharAtCursor(ch);
        isSaved = 0;
      }
    }
    break;
  case WM_KEYDOWN:
    if (mode == Insert) {
      if (wParam == VK_ESCAPE)
        mode = Normal;
    } else if (mode == Normal) {
      if (wParam == VK_ESCAPE)
        ClearCommand();
    }
    /*if (mode == Normal) {
      if (wParam == VK_ESCAPE) {
        PostQuitMessage(0);
        isRunning = 0;
      }
      if (wParam == VK_RETURN) {
        InsertCharAtCursor('\n');
        isSaved = 0;
      }
      if (wParam == 'I') {
        mode = Insert;
        isJustEnteredInsert = 1;
      }
      if (wParam == VK_BACK) {
        RemoveCurrentChar();
        isSaved = 0;
      }
      if (wParam == 'S' &&
    IsKeyPressed(VK_CONTROL))
        SaveFile();
      if (wParam == 'J')
        MoveDown();
      if (wParam == 'K')
        MoveUp();
      if (wParam == 'W')
        JumpWordForward();
      if (wParam == 'B')
        JumpWordBackward();
      if (wParam == 'L')
        cursorPos++;
      if (wParam == 'H')
        cursorPos--;
    } else if (mode == Insert) {
      if (wParam == VK_RETURN) {
        InsertCharAtCursor('\n');
        isSaved = 0;
      }
      if (wParam == VK_BACK) {
        RemoveCurrentChar();
        isSaved = 0;
      }
      if (wParam == VK_ESCAPE)
        mode = Normal;
    }
    */
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    isRunning = 0;
    break;
  case WM_SIZE:
    width = LOWORD(lParam);
    height = HIWORD(lParam);

    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    // makes rows go down, instead of
    // going up by default
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    canvas.width = width;
    canvas.height = height;
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

FontData fontD;
Rect rect = {0};

void DrawFooter() {
  char *label = filePath;
  int padding = 2;
  int len = strlen(label);
  int footerHeight = fontD.charHeight + padding * 2;
  PaintRect(0, rect.height - footerHeight, rect.width, footerHeight, 0x202020);
  // int x = rect.width -
  // fontD.charWidth * (len + 1) -
  // padding;
  int x = padding;
  int y = rect.height - fontD.charHeight - padding;
  while (*label) {
    CopyMonochromeTextureRectTo(&canvas, &rect, &fontD.textures[*label], x, y, 0xffffff);
    x += fontD.charWidth;
    label++;
  }
  if (!isSaved)
    CopyMonochromeTextureRectTo(&canvas, &rect, &fontD.textures['*'], x, y, 0xffffff);

  int charsToShow = currentCommandLen > 0 ? currentCommandLen : visibleCommandLen;
  int commandX = rect.width / 2 - (charsToShow * fontD.charWidth) / 2;
  for (int i = 0; i < charsToShow; i++) {
    u32 color = currentCommandLen > 0 ? 0xffffff : 0xbbbbbb;
    CopyMonochromeTextureRectTo(&canvas, &rect, &fontD.textures[currentCommand[i]], commandX, y, color);
    commandX += fontD.charWidth;
  }
}

void Draw() {
  for (i32 i = 0; i < canvas.width * canvas.height; i++)
    canvas.pixels[i] = colorsBg;

  rect.x = 0;
  rect.y = 0;
  rect.width = width;
  rect.height = height;
  i32 padding = 10;

  i32 x = padding;
  i32 y = padding;

  u32 cursorColor = mode == Normal ? colorsCursorInsert : colorsCursorNormal;
  CursorPos cursor = GetCursorPosition();
  i32 cursorX = x + fontD.charWidth * cursor.lineOffset;
  i32 cursorY = y + fontD.charHeight * cursor.line;

  u32 lineColor = mode == Normal ? colorsCursorLineNormal : colorsCursorLineInsert;
  PaintRect(0, cursorY, rect.width, fontD.charHeight, lineColor);
  PaintRect(cursorX, cursorY, fontD.charWidth, fontD.charHeight, cursorColor);

  for (i32 i = 0; i < file.size; i++) {

    char ch = file.content[i];
    if (ch == '\n') {
      x = padding;
      y += fontD.charHeight;
    } else if (ch < MAX_CHAR_CODE && ch >= ' ') {
      CopyMonochromeTextureRectTo(&canvas, &rect, &fontD.textures[ch], x, y, colorsFont);
      x += fontD.charWidth;
    } else {
      PaintRect(x, y, fontD.charWidth, fontD.charHeight, 0xff0000);
      x += fontD.charWidth;
    }
  }

  DrawFooter();
  StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height, canvas.pixels,

                &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

StringBuffer ReadFileIntoDoubledSizedBuffer(char *path) {
  u32 fileSize = GetMyFileSize(path);
  StringBuffer res = {.capacity = fileSize * 2, .size = fileSize, .content = 0};
  res.content = VirtualAllocateMemory(res.capacity);
  ReadFileInto(path, fileSize, res.content);

  // removing windows new lines
  // delimeters, assuming no two CR are
  // next to each other
  for (int i = 0; i < fileSize; i++) {
    if (*(res.content + i) == '\r')
      RemoveCharAt(&res, i);
  }

  PlaceLineEnd(&res);
  return res;
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
  file = ReadFileIntoDoubledSizedBuffer(filePath);

  InitFontSystem();
  Arena fontArena = CreateArena(500 * 1024);
  InitFont(&fontD, "Consolas", 14, &fontArena);
  i64 startCounter = GetPerfCounter();
  while (isRunning) {
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    Draw();
    Sleep(10);
    i64 endCounter = GetPerfCounter();
    appTimeMs += ((f32)(endCounter - startCounter) * 1000.0f / (f32)GetPerfFrequency());
    startCounter = endCounter;
  }
  return 0;
}
