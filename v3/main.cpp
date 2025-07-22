#include "commands.cpp"
#include "math.cpp"
#include "text.cpp"
#include "vim.cpp"
#include "win32.cpp"
#include <stdio.h>

Window window;
u32 color = 0xcccccc;
u32 bg = 0x000000;
u32 bgEven = 0x444444;
u32 footerBg = 0x111111;
u32 linesColor = 0x191919;

i32 hPadding = 20; // set to char width
i32 vPadding = 8;
f32 lineHeight = 1.1f;
TEXTMETRIC textMetric;

HBITMAP canvasBitmap;
HDC currentDc;

Mode mode = Normal;
HFONT smallFont;
HFONT font;
HFONT titleFont;

i32 ignoreNextCharEvent; // this is used to ignore char event which comes immeidatelly after
f32 buildTimeMs;
// entering insert mode

Buffer* selectedBuffer;
Buffer leftBuffer;
Buffer buffer;
Buffer textBuffer;
Buffer outBuffer;

enum OutBufferTab { Stats = 0, Output, Logs, Errors };
OutBufferTab selectedTab = Stats;

i32 hasErrors;
char* output;
i32 outputLen;

void UpdateCursor(i32 c) {}

u32 ToWinColor(u32 color) {
  return ((color & 0xff0000) >> 16) | (color & 0x00ff00) | ((color & 0x0000ff) << 16);
}

void TextColors(u32 foreground, u32 background) {
  SetBkColor(currentDc, ToWinColor(background));
  SetTextColor(currentDc, ToWinColor(foreground));
}

void ReadFileIntoDoubledSizedBuffer(Buffer* b) {
  i64 filesize = GetMyFileSize(b->path);
  char* filetemp = (char*)VirtualAllocateMemory(filesize);
  ReadFileInto(b->path, filesize, filetemp);

  b->capacity = filesize * 2;
  b->size = 0;
  b->file = (char*)VirtualAllocateMemory(b->capacity);
  b->isSaved = true;
  for (i32 i = 0; i < filesize; i++) {
    if (filetemp[i] != '\r')
      b->file[b->size++] = filetemp[i];
  }
  VirtualFreeMemory(filetemp);
}

void InitFileBuffer(const char* path, Buffer* b) {
  b->path = path;
  b->changes.capacity = KB(40);
  b->changes.contents = (Change*)VirtualAllocateMemory(b->changes.capacity);
  b->changes.lastChangeIndex = -1;

  ReadFileIntoDoubledSizedBuffer(b);
}

HFONT CreateFont(i32 fontSize, i32 weight) {

  int h = -MulDiv(fontSize, GetDeviceCaps(window.dc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
  return CreateFontA(h, 0, 0, 0,
                     // FW_BOLD,
                     weight,
                     0, // Italic
                     0, // Underline
                     0, // Strikeout
                     DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH, "Consolas");
}

void Init() {
  InitFileBuffer("main.cpp", &leftBuffer);
  InitFileBuffer("console.cpp", &buffer);
  InitFileBuffer("progress.txt", &textBuffer);
  selectedBuffer = &buffer;
  output = (char*)VirtualAllocateMemory(MB(1));

  smallFont = CreateFont(12, FW_REGULAR);
  font = CreateFont(14, FW_REGULAR);
  titleFont = CreateFont(18, FW_BOLD);

  SelectObject(currentDc, font);
  GetTextMetricsA(currentDc, &textMetric);
}

void InsertCharAt(char ch, i32 pos) {
  InsertChars(selectedBuffer, &ch, 1, pos);
}

void InsertCharAtCursor(char ch) {
  InsertCharAt(ch, selectedBuffer->cursorPos);
  UpdateCursor(selectedBuffer->cursorPos + 1);
}

// void AddLineAbove() {
//   i32 offset = GetOffsetForLineAt(selectedBuffer, selectedBuffer->cursorPos);
//   i32 start = FindLineStart(selectedBuffer, selectedBuffer->cursorPos);
//   InsertCharAt('\n', start);

//   for (i32 i = 0; i < offset; i++)
//     InsertCharAt(' ', start + i);
//   UpdateCursor(start + offset);
//   mode = Insert;
//   ignoreNextCharEvent = 1;
// }

void AddLineBelow() {
  i32 offset = GetOffsetForLineAt(selectedBuffer, selectedBuffer->cursorPos);
  i32 end = FindLineEnd(selectedBuffer, selectedBuffer->cursorPos);
  InsertCharAt('\n', end);

  for (i32 i = 0; i < offset; i++)
    InsertCharAt(' ', end + i + 1);
  UpdateCursor(end + offset + 1);
  mode = Insert;
  ignoreNextCharEvent = 1;
}

void AddLineInside() {
  i32 offset = GetOffsetForLineAt(selectedBuffer, selectedBuffer->cursorPos);
  i32 end = FindLineEnd(selectedBuffer, selectedBuffer->cursorPos);
  InsertCharAt('\n', end);
  offset += 2;

  for (i32 i = 0; i < offset; i++)
    InsertCharAt(' ', end + i + 1);
  UpdateCursor(end + offset + 1);
  mode = Insert;
  ignoreNextCharEvent = 1;
}

void SaveBuffer(Buffer* buffer) {
  if (buffer->path) {
    WriteMyFile((char*)buffer->path, buffer->file, buffer->size);
    buffer->isSaved = true;
  }
}

void Run() {
  i64 start = GetPerfCounter();
  SaveBuffer(&leftBuffer);
  SaveBuffer(&buffer);

  char* s = (char*)"cmd /c clang console.cpp";
  RunCommand(s, output, &outputLen);

  hasErrors = outputLen != 0;
  if (!hasErrors) {

    char* s2 = (char*)"cmd /c a.exe";
    RunCommand(s2, output, &outputLen);
    selectedTab = Output;
  } else {
    selectedTab = Errors;
  }
  i64 end = GetPerfCounter();
  buildTimeMs = f32(end - start) * 1000.0f / (f32)GetPerfFrequency();
}

void FormatCode() {
  SaveBuffer(selectedBuffer);
  char s[255] = {0};
  sprintf(s, "cmd /c clang-format console.cpp --cursor=%d", selectedBuffer->cursorPos);
  RunCommand(s, output, &outputLen);
  char* t = output;
  i32 start = 0;
  while (t[start] != '\n')
    start++;
  start++;
  i32 newLen = outputLen - start;
  // Replace text action
  memmove(selectedBuffer->file, output + start, newLen);
  selectedBuffer->size = newLen;
  selectedBuffer->isSaved = false;

  // 12 here is the index of new cursor position in JSON response { "Cursor": 36
  // I don;t want to parse entire JSON yet
  int newCursor = atoi(output + 12);
  UpdateCursor(newCursor);
}

void RemoveChar() {
  if (selectedBuffer->cursorPos > 0) {
    RemoveChars(selectedBuffer, selectedBuffer->cursorPos - 1, selectedBuffer->cursorPos - 1);
    UpdateCursor(selectedBuffer->cursorPos - 1);
  }
}

void RemoveCharFromRight() {
  if (selectedBuffer->cursorPos < (selectedBuffer->size - 1)) {
    RemoveChars(selectedBuffer, selectedBuffer->cursorPos, selectedBuffer->cursorPos);
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

void CopySelection() {
  // SelectionRange range = GetSelectionRange();
  // ClipboardCopy(window.windowHandle, selectedBuffer->file + range.start,
  //               range.end - range.start + 1);
}

void PasteFromClipboard() {
  i32 size = 0;
  char* textBuffer = ClipboardPaste(window.windowHandle, &size);
  InsertChars(selectedBuffer, textBuffer, size, selectedBuffer->cursorPos);
  selectedBuffer->cursorPos += size;

  VirtualFreeMemory(textBuffer);
}

void FinishCommand() {
  currentCommandLen = 0;
}

bool IsSingleCharAlt(char ch) {
  return currentCommand[0].ch == 'f' && currentCommand[0].alt;
}

// void TryRunCommand() {
//   Key lastKey = currentCommand[currentCommandLen];
//   if (mode == Insert && IsPrintable(lastKey.ch)) {
//     InsertCharAtCursor(lastKey.ch);
//   } else {
//     if (IsLastCommand(VK_ESCAPE)) {
//       if (mode != Normal) {
//         if (mode == Insert)
//           FinishAndSaveCommand();

//         mode = Normal;
//       }
//     }
//     if (IsLastCommand('i'))
//       mode = Insert;
//     if (IsLastCommand('v')) {
//       mode = Visual;
//       selectedBuffer->selectionStart = selectedBuffer->cursorPos;
//     }
//     if (IsLastCommand('V')) {
//       mode = VisualLine;
//       selectedBuffer->selectionStart = selectedBuffer->cursorPos;
//     }

//     if (IsLastCommand('d')) {
//       if (mode == Visual || mode == VisualLine) {
//         RemoveSelection();
//         mode = Normal;
//         FinishAndSaveCommand();
//       }
//     }
//     if (IsSingleCharAlt('f')) {
//       FormatCode();
//       FinishCommand();
//     }
//     if (IsSingleCharAlt('r')) {
//       Run();
//       FinishCommand();
//     }

//     if (IsLastCommand('X'))
//       RemoveChar();
//     if (IsLastCommand('x'))
//       RemoveCharFromRight();
//     if (IsLastCommand('o')) {
//       if (mode == Normal)
//         AddLineBelow();
//       else if (mode == Visual || mode == VisualLine) {
//         i32 temp = selectedBuffer->selectionStart;
//         selectedBuffer->selectionStart = selectedBuffer->cursorPos;
//         UpdateCursor(temp);
//       }
//     }
//     if (IsLastCommand('c') && (mode == Visual || mode == VisualLine)) {
//       RemoveSelection();
//       AddLineAbove();
//     }

//     if (IfSingleCommandFinish("O"))
//       AddLineAbove();
//     if (IfSingleCommandFinish("q"))
//       Quit();
//     if (IfSingleCommandFinish("l"))
//       MoveRight();
//     if (IfSingleCommandFinish("h"))
//       MoveLeft();
//     if (IfSingleCommandFinish("j"))
//       MoveDown();
//     if (IfSingleCommandFinish("k"))
//       MoveUp();
//     if (IfSingleCommandFinish("B"))
//       UpdateCursor(JumpWordWithPunctuationBackward(selectedBuffer));
//     if (IfSingleCommandFinish("b"))
//       UpdateCursor(JumpWordBackward(selectedBuffer));
//     if (IfSingleCommandFinish("W"))
//       UpdateCursor(JumpWordWithPunctuationForward(selectedBuffer));
//     if (IfSingleCommandFinish("w"))
//       UpdateCursor(JumpWordForward(selectedBuffer));
//   }
// }

void RepeartLastCommand();
void AppendKey2(Key key) {
  if (key.ch == '.' && currentCommandLen == 0) {
    RepeartLastCommand();
  } else {
    currentCommand[currentCommandLen] = key;
    currentCommandLen++;
    // TryRunCommand();
  }
}

void RepeartLastCommand() {
  currentCommandLen = 0;
  for (i32 i = 0; i < lastCommandLen; i++) {
    AppendKey(lastCommand[i], selectedBuffer, &mode, &window);
  }
}

void OnKeyPressed(u64 ch) {
  Key key = {
      .ctrl = IsKeyPressed(VK_CONTROL),
      .alt = IsKeyPressed(VK_MENU),
      .ch = ch,
  };

  AppendKey(key, selectedBuffer, &mode, &window);
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
  case WM_SYSCHAR:
  case WM_CHAR: {
    if (IsPrintable(wParam))
      OnKeyPressed(wParam);
    // if (ignoreNextCharEvent)
    //   ignoreNextCharEvent = 0;
    // else if (mode == Insert && wParam >= ' ' && wParam <= 126)
    //   InsertCharAtCursor(wParam);
    break;
  }
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    // assume this won't generate WM_CHAR nor WM_SYSCHAR events by OS
    if (wParam == VK_ESCAPE || (IsKeyPressed(VK_CONTROL) && IsPrintable(wParam)))
      OnKeyPressed(wParam);
    // if (wParam == VK_F11) {
    //   window.isFullscreen = !window.isFullscreen;
    //   SetFullscreen(window.windowHandle, window.isFullscreen);
    // }
    // if (mode == Normal) {
    //   HandleNormalAndVisualCommands(wParam);

    //   if (wParam >= '1' && wParam <= '4') {
    //     if (IsKeyPressed(VK_SHIFT) && IsKeyPressed(VK_MENU)) {
    //       if (wParam == '1')
    //         selectedTab = Stats;
    //       if (wParam == '2')
    //         selectedTab = Output;
    //       if (wParam == '3')
    //         selectedTab = Logs;
    //       if (wParam == '4')
    //         selectedTab = Errors;
    //     } else if (IsKeyPressed(VK_MENU)) {

    //       if (wParam == '1')
    //         selectedBuffer = &leftBuffer;
    //       if (wParam == '2')
    //         selectedBuffer = &buffer;
    //       if (wParam == '4')
    //         selectedBuffer = &textBuffer;
    //     }
    //     // if(wParam == '1')
    //     // selectedBuffer = &leftBuffer;
    //   }
    //   if (wParam == 'V') {
    //     selectionStart = selectedBuffer->cursorPos;
    //     mode = Visual;
    //   }
    //   if (wParam == 'I') {
    //     mode = Insert;
    //     ignoreNextCharEvent = 1;
    //   }
    //   if (wParam == 'S' && IsKeyPressed(VK_MENU))
    //     SaveBuffer(selectedBuffer);
    //   if (wParam == 'O' && IsKeyPressed(VK_CONTROL))
    //     AddLineInside();
    //   else if (wParam == 'O' && IsKeyPressed(VK_SHIFT))
    //     AddLineAbove();
    //   else if (wParam == 'O')
    //     AddLineBelow();
    //   if (wParam == 'R' && IsKeyPressed(VK_MENU))
    //     Run();

    //   if (wParam == VK_BACK)
    //     RemoveChar();
    //   if (wParam == 'D')
    //     RemoveCharFromRight();
    //   if (wParam == 'P')
    //     PasteFromClipboard();

    //   if (wParam == 'U' && IsKeyPressed(VK_SHIFT)) {
    //     Change* c = RedoLastChange(selectedBuffer);

    //     if (c)
    //       UpdateCursor(c->at);
    //   } else if (wParam == 'U') {
    //     Change* c = UndoLastChange(selectedBuffer);
    //     if (c)
    //       UpdateCursor(c->at);
    //   }
    //   if (wParam == VK_RETURN)
    //     InsertCharAtCursor('\n');
    // } else if (mode == Insert) {
    //   if (wParam == VK_ESCAPE)
    //     mode = Normal;
    //   if (wParam == VK_BACK) {
    //     RemoveChar();
    //   }
    //   if (wParam == 'V' && IsKeyPressed(VK_CONTROL))
    //     PasteFromClipboard();
    //   if (wParam == VK_RETURN)
    //     InsertCharAtCursor('\n');
    // } else if (mode == Visual) {
    //   HandleNormalAndVisualCommands(wParam);

    //   if (wParam == VK_ESCAPE) {
    //     mode = Normal;
    //     selectionStart = -1;
    //   }
    //   if (wParam == 'D') {
    //     RemoveSelection();
    //     mode = Normal;
    //     selectionStart = -1;
    //   }
    //   if (wParam == 'Y') {
    //     CopySelection();
    //   }
    //   if (wParam == 'P') {
    //     RemoveSelection();
    //     mode = Normal;
    //     selectionStart = -1;
    //     PasteFromClipboard();
    //   }
    // }
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

void PrintParagraph(i32 x, i32 y, char* ch, i32 len) {
  i32 line = 0;
  i32 lineStart = 0;
  while (lineStart < len) {
    i32 lineEnd = lineStart;
    while (lineEnd < len && ch[lineEnd] != '\n')
      lineEnd++;

    TextColors(color, bg);

    TextOutA(currentDc, x, y, ch + lineStart, lineEnd - lineStart);
    lineStart = lineEnd + 1;
    lineEnd += 2;
    line++;
    y += textMetric.tmHeight * lineHeight;
  }
}

i32 FormatKey(u64 ch, char* buff) {
  if (IsPrintable(ch))
    return sprintf(buff, "%c", (char)ch);
  else {

    const char* s = "<NAN>";

    if (ch == VK_ESCAPE)
      s = "<Esc>";

    return sprintf(buff, "%s", s);
  }
}

i32 FormatCommand(Key* commands, i32 len, char* buffer) {
  i32 c = 0;
  for (i32 i = 0; i < len; i++) {
    char ch = commands[i].ch;
    char keyBuff[64] = {0};
    FormatKey(ch, keyBuff);

    if (commands[i].ctrl && commands[i].alt)
      c += sprintf(buffer + c, "<CA-%s>", keyBuff);
    else if (commands[i].ctrl)
      c += sprintf(buffer + c, "<C-%s>", keyBuff);
    else if (commands[i].alt)
      c += sprintf(buffer + c, "<A-%s>", keyBuff);
    else {
      c += sprintf(buffer + c, "%s", keyBuff);
    }
  }
  return c;
}

f32 measurements[100];
i32 currentMeasurement;
void PrintFrameStats(Rect rect) {
  TextColors(0xdddddd, bg);
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

  char statsFormatted[255] = {0};
  char commandFormatted[255] = {0};
  FormatCommand(currentCommand, currentCommandLen, commandFormatted);
  char lastCommandFormatted[255] = {0};
  FormatCommand(lastCommand, lastCommandLen, lastCommandFormatted);
  i32 l = sprintf(statsFormatted,
                  "Frame: %0.1fms  Build: %0.1fms\nBuffer len: %d, cap: %d\nChanges current: %d, "
                  "total: %d, occupied: %d, cap: %d\nCommand: %s\nLast command: %s",
                  average * 1000.0f, buildTimeMs, selectedBuffer->size, selectedBuffer->capacity,
                  selectedBuffer->changes.lastChangeIndex, selectedBuffer->changes.changesCount,
                  GetChangeArenaSize(&selectedBuffer->changes), selectedBuffer->changes.capacity,
                  commandFormatted, lastCommandFormatted);
  PrintParagraph(rect.x + hPadding, rect.y + vPadding, statsFormatted, l);
  // TextOutA(currentDc, 10, window.height - textMetric.tmHeight * lineHeight, &foo[0], l);
}

struct TextPos {
  i32 line;
  i32 offset;
};
TextPos GetTextPos(i32 pos) {
  TextPos res = {0};
  for (i32 i = 0; i < pos; i++) {
    if (selectedBuffer->file[i] == '\n') {
      res.line++;
      res.offset = 0;
    } else {
      res.offset++;
    }
  }
  return res;
}

i32 CharWidth() {
  // assumes monospace font
  SIZE size;
  GetTextExtentPoint32W(currentDc, (LPCWSTR) "w", 1, &size);
  return size.cx;
}

void ShowBufferInRect(Rect rect, Buffer* buffer) {
  SelectFont(currentDc, titleFont);
  u32 fileColor = buffer->isSaved ? 0x2222222 : 0x402222;
  TextColors(fileColor, bg);
  i32 titleWidth = strlen(buffer->path) * CharWidth() + 10.0f;
  TextOut(currentDc, rect.x + rect.width - titleWidth, rect.y + 10, buffer->path,
          strlen(buffer->path));

  SelectFont(currentDc, font);
  i32 lineStart = 0;
  i32 len = buffer->size;
  i32 x = rect.x + hPadding;
  f32 y = rect.y + vPadding;

  i32 line = 0;
  char* ch = buffer->file;

  i32 charWidth = CharWidth();

  while (lineStart < len) {
    i32 lineEnd = lineStart;
    while (lineEnd < len && ch[lineEnd] != '\n')
      lineEnd++;

    TextColors(color, bg);

    TextOutA(currentDc, x, y, ch + lineStart, lineEnd - lineStart);
    lineStart = lineEnd + 1;
    lineEnd += 2;
    line++;
    y += textMetric.tmHeight * lineHeight;
  }

  if (selectedBuffer == buffer) {

    TextPos textPos = GetTextPos(selectedBuffer->cursorPos);
    f32 cursorX = rect.x + hPadding + (f32)textPos.offset * (f32)charWidth;
    f32 cursorY = rect.y + vPadding + textPos.line * textMetric.tmHeight * lineHeight;

    // print selection
    if ((mode == Visual || mode == VisualLine) && selectedBuffer->selectionStart >= 0) {

      SelectionRange range = GetSelectionRange(selectedBuffer, mode);
      i32 selectionLeft = range.start;
      i32 selectionRight = range.end;

      TextPos sel1 = GetTextPos(selectionLeft);
      f32 x1 = rect.x + hPadding + (f32)sel1.offset * (f32)charWidth;
      f32 y1 = rect.y + vPadding + sel1.line * textMetric.tmHeight * lineHeight;
      TextColors(0xeeeeee, 0x232D39);

      while (selectionLeft <= selectionRight) {
        i32 lineEnd = MinI32(FindLineEnd(selectedBuffer, selectionLeft), selectionRight);

        TextOutA(currentDc, x1, y1, ch + selectionLeft, lineEnd - selectionLeft + 1);
        selectionLeft = lineEnd + 1;
        x1 = rect.x + hPadding;
        y1 += textMetric.tmHeight * lineHeight;
      }
    }

    u32 textColorUnderCurosr = 0x000000;
    // print cursor
    if (mode == Normal || mode == Visual || mode == VisualLine) {
      u32 cursorColor = 0xFFDC32;
      if (mode == Visual || mode == VisualLine)
        cursorColor = 0x32DCff;
      PaintRect(V2{cursorX, cursorY}, charWidth, textMetric.tmHeight, cursorColor);

      TextColors(textColorUnderCurosr, cursorColor);
      TextOutA(currentDc, cursorX, cursorY, ch + selectedBuffer->cursorPos, 1);
    } else if (mode == Insert) {
      f32 barWidth = 2.0f;
      PaintRect(V2{cursorX - barWidth / 2, cursorY}, barWidth, textMetric.tmHeight, 0xff2222);
    }
  }
}

void PrintTopRight(Rect rect) {

  SelectFont(currentDc, smallFont);

  const char* labels[] = {"Stats", "Output", "Logs", "Errors"};
  int selectedLabel = selectedTab;

  i32 charWidth = CharWidth();
  i32 x = rect.x + rect.width;
  i32 y = rect.y;
  for (i32 i = ArrayLength(labels) - 1; i >= 0; i--) {
    const char* label = labels[i];
    x -= strlen(label) * charWidth;

    if (i == selectedLabel)
      TextColors(0xeeeeee, bg);
    else
      TextColors(0x888888, bg);
    TextOut(currentDc, x, y, label, strlen(label));

    x -= charWidth;
  }

  if (selectedTab == Stats)
    PrintFrameStats(rect);
  else if (selectedTab == Errors)
    PrintParagraph(rect.x + hPadding, rect.y + vPadding, output, outputLen);
  else if (selectedTab == Output && outputLen > 0)
    PrintParagraph(rect.x + hPadding, rect.y + vPadding, output, outputLen);
}

void UpdateAndDraw(f32 deltaSec) {
  Rect left = {0, 0, i32(window.width / 3.0f), window.height};
  Rect middle = {left.width, 0, i32(window.width / 3.0f), window.height};
  i32 rightWidth = window.width - (middle.x + middle.width);
  Rect right = {middle.x + middle.width, 0, rightWidth, window.height};
  Rect topRight = {right.x, right.y, right.width, right.height / 2};
  Rect bottomRight = {right.x, topRight.y + topRight.height, right.width,
                      window.height - topRight.height};

  PaintRect(V2{(f32)middle.x - 1.0f, 0}, 2, window.height, linesColor);
  PaintRect(V2{(f32)right.x - 1.0f, 0}, 2, window.height, linesColor);
  PaintRect(V2{(f32)bottomRight.x, bottomRight.y - 1.0f}, bottomRight.width, 2, linesColor);

  ShowBufferInRect(left, &leftBuffer);

  ShowBufferInRect(middle, &buffer);

  ShowBufferInRect(bottomRight, &textBuffer);

  PrintTopRight(topRight);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  PreventWindowsDPIScaling();

  window = {.width = 1400,
            .height = 1100,
            .bg = 0x222222,
            .title = "Editor",
            .isFullscreen = 1,
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
