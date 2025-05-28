#include "anim.c"
#include "font.c"
#include "math.c"
#include "search.c"
#include "vim.c"
#include "win32.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int isRightBufferVisible = 1;
HWND mainWindow;
int isRunning = 1;
int isFullscreen = 0;
// u32 colorsBg = 0x0F1419;
u32 colorsBg = 0x121212;
u32 colorsFont = 0xE6E1CF; // 0xCCCCCC;
u32 colorsFooter = 0x1D1D1D;

u32 colorsCursorNormal = 0xFFDC32;
u32 colorsCursorInsert = 0xFC2837;
u32 colorsCursorVisual = 0x3E9FBE;
u32 colorsCursorLocalSearchType = 0x3e9f9f;

u32 colorsCursorLineNormal = 0x3F370E;
u32 colorsCursorLineInsert = 0x3D2425;
u32 colorsCursorLineVisual = 0x202020;
u32 colorsCursorLineLocalSearchType = 0x203030;

u32 colorsSearchResult = 0x226622;
u32 colorsSelection = 0x253340;
u32 colorsScrollbar = 0x888888;

float scrollbarWidth = 10;

Rect leftRect;
Spring leftOffset;

Rect compilationRect;
Spring compilationOffset;

Rect middleRect;
Spring middleOffset;

Rect rightRect;
Spring rightOffset;

Rect footerRect = {0};
i32 horizPadding = 20;
i32 vertPadding = 10;
i32 footerPadding = 2;
Rect screen = {0};

char* rootDir = ".\\";
Buffer leftBuffer;
char* leftFilePath = ".\\misc\\tasks.txt";

Buffer middleBuffer;
char* middleFilePath = ".\\str.c";

Buffer compilationOutputBuffer;
Buffer rightBuffer;
char* rightFilePath = ".\\vim.c";

char* allFiles[] = {"main.c",   "font.c", "anim.c",  "math.c",         "tags",
                    "search.c", "vim.c",  "win32.c", "misc\\build.bat"};

typedef enum EdFile { Left, Middle, CompilationResults, Right } EdFile;

Buffer* selectedBuffer;
Spring* selectedOffset;
Rect* selectedRect;
EdFile selectedFile;

void SelectFile(EdFile file) {
  if (!isRightBufferVisible && file == Right)
    return;
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

typedef enum Mode {
  Normal,
  Insert,
  ReplaceChar,
  Visual,
  VisualLine,
  FileSelection,
  LocalSearchTyping
} Mode;
f32 appTimeMs = 0;
Mode mode = Normal;

BITMAPINFO bitmapInfo;
HDC dc;
Key currentCommand[512];
i32 currentCommandLen;
i32 visibleCommandLen;
FontData font;
Arena fontArena;

i32 lineHeightPx;
f32 lineHeight = 1.1;
i32 fontSize = 14;
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

int GetMatchingCharIndex() {
  char* text = selectedBuffer->content;
  int pos = selectedBuffer->cursor;

  char currentChar = text[pos];
  char oposingChar = 0;
  if (currentChar == '(')
    oposingChar = ')';

  if (currentChar == '{')
    oposingChar = '}';

  if (currentChar == '[')
    oposingChar = ']';

  if (currentChar == ')')
    oposingChar = '(';

  if (currentChar == '}')
    oposingChar = '{';

  if (currentChar == ']')
    oposingChar = '[';

  if (!oposingChar)
    return -1;

  int isGoingForwards = currentChar == '(' || currentChar == '{' || currentChar == '[';

  int numberOfScopes = 0;
  if (isGoingForwards) {
    while (pos < selectedBuffer->size) {
      if (text[pos] == currentChar)
        numberOfScopes++;
      if (text[pos] == oposingChar)
        numberOfScopes--;

      if (text[pos] == oposingChar && numberOfScopes == 0) {
        break;
      }
      pos++;
    }
    return pos;
  } else {
    while (pos >= 0) {
      if (text[pos] == oposingChar)
        numberOfScopes++;
      if (text[pos] == currentChar)
        numberOfScopes--;

      if (text[pos] == oposingChar && numberOfScopes == 0) {
        break;
      }
      pos--;
    }
    return pos;
  }
  return -1;
}

void SetCursorPosition(i32 v) {
  if (v < 0)
    return;
  selectedBuffer->cursor = Clampi32(v, 0, selectedBuffer->size - 1);
  selectedBuffer->oposingCharAt = GetMatchingCharIndex();
  CursorPos cursor = GetCursorPosition(selectedBuffer);

  float lineToLookAhead = 5.0f * lineHeightPx;
  float cursorPos = cursor.line * lineHeightPx;
  float maxScroll = GetPageHeight(selectedBuffer) - selectedRect->height;
  if ((selectedOffset->target + selectedRect->height - lineToLookAhead) < cursorPos)
    selectedOffset->target = Clamp(cursorPos - (float)selectedRect->height / 2.0f, 0, maxScroll);
  else if ((selectedOffset->target + lineToLookAhead) > cursorPos)
    selectedOffset->target = Clamp(cursorPos - (float)selectedRect->height / 2.0f, 0, maxScroll);
}

void SaveSelectedFile() {
  WriteMyFile(selectedBuffer->filePath, selectedBuffer->content, selectedBuffer->size);
  selectedBuffer->isSaved = 1;
}

void LoadFileIntoSelectedBuffer(char* path) {
  SaveSelectedFile();
  VirtualFreeMemory(selectedBuffer->content);
  VirtualFreeMemory(selectedBuffer->changeArena.contents);
  *selectedBuffer = ReadFileIntoDoubledSizedBuffer(path);
  InitChangeArena(selectedBuffer);
  mode = Normal;
  selectedOffset->target = selectedOffset->current = 0;
}

void FocusOnEntry(int index) {
  SetCursorPosition(entriesAt[index].at);
  currentEntry = index;
}

void FindClosestMatch() {
  if (entriesCount > 0) {
    if (selectedBuffer->cursor < entriesAt[0].at)
      FocusOnEntry(0);
    else {
      for (int i = 1; i < entriesCount; i++) {
        if (entriesAt[i - 1].at < selectedBuffer->cursor &&
            entriesAt[i].at >= selectedBuffer->cursor) {
          FocusOnEntry(i);
          return;
        }
      }
      FocusOnEntry(0);
    }
  }
}

void MoveNextOnSearch() {
  if (entriesCount > 0) {
    if (selectedBuffer->cursor < entriesAt[0].at)
      FocusOnEntry(0);
    else {
      for (int i = 1; i < entriesCount; i++) {
        if (entriesAt[i - 1].at <= selectedBuffer->cursor &&
            entriesAt[i].at > selectedBuffer->cursor) {
          FocusOnEntry(i);
          return;
        }
      }
      FocusOnEntry(0);
    }
  }
}

void MovePrevOnSearch() {
  if (entriesCount > 0) {
    if (selectedBuffer->cursor < entriesAt[0].at)
      FocusOnEntry(entriesCount - 1);
    else {
      for (int i = 1; i < entriesCount; i++) {
        if (entriesAt[i - 1].at < selectedBuffer->cursor &&
            entriesAt[i].at >= selectedBuffer->cursor) {
          FocusOnEntry(i - 1);
          return;
        }
      }
      FocusOnEntry(entriesCount - 1);
    }
  }
}

void StartSearch() {
  isSearchVisible = 1;
  FindEntries(selectedBuffer);

  FindClosestMatch();
}

typedef struct SelectionRange {
  i32 left;
  i32 right;
} SelectionRange;

SelectionRange GetSelectionRange() {
  i32 selectionLeft = MinI32(selectedBuffer->selectionStart, selectedBuffer->cursor);
  i32 selectionRight = MaxI32(selectedBuffer->selectionStart, selectedBuffer->cursor);
  if (mode == VisualLine) {
    selectionLeft = FindLineStart(selectedBuffer, selectionLeft);
    selectionRight = FindLineEnd(selectedBuffer, selectionRight);
  }

  SelectionRange res;
  res.left = selectionLeft;
  res.right = selectionRight;
  return res;
}

void OnLayout() {
  int footerHeight = font.charHeight + footerPadding * 2;
  lineHeightPx = RoundI32((f32)font.charHeight * lineHeight);
  footerRect.width = screen.width;
  footerRect.height = footerHeight;
  footerRect.y = screen.height - footerRect.height;

  compilationRect.width = leftRect.width = 70 * font.charWidth;
  compilationRect.height = 800;

  leftRect.height = canvas.height - compilationRect.height - footerRect.height;
  compilationRect.y = leftRect.height;

  i32 codeWidth = (screen.width - leftRect.width) / 2;
  middleRect.x = leftRect.width;
  if (isRightBufferVisible) {
    middleRect.width = canvas.width / 3;
    rightRect.width = canvas.width - (leftRect.width + middleRect.width);
  } else {
    middleRect.width = canvas.width - leftRect.width;
  }

  middleRect.height = canvas.height - footerRect.height;

  rightRect.x = middleRect.x + middleRect.width;
  rightRect.height = canvas.height - footerRect.height;
}

void ResetAppFonts() {
  InitFont(&font, fontName, fontSize, &fontArena);
  OnLayout();
}

void CenterViewOnCursor() {

  CursorPos cursor = GetCursorPosition(selectedBuffer);
  float cursorPos = cursor.line * lineHeightPx;

  selectedOffset->target = cursorPos - (float)selectedRect->height / 2.0f;
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

void FormatSelectedFile() {

  SaveSelectedFile();
  char* style = "--style=\"{AllowShortFunctionsOnASingleLine : \"Empty\", "
                "ColumnLimit: 100, PointerAlignment: \"Left\"}\"";
  char cmd[512] = {0};
  sprintf(cmd, "cmd /c clang-format %s -cursor=%d %s", style, selectedBuffer->cursor,
          selectedBuffer->filePath);

  i32 len = 0;
  char* output = VirtualAllocateMemory(selectedBuffer->size + 200);
  RunCommand(cmd, output, &len);

  char* nextTextStart = output;
  while (*nextTextStart != '\n')
    nextTextStart++;
  nextTextStart++;

  // 12 here is the index of new cursor position in JSON response { "Cursor": 36
  // I don;t want to parse entire JSON yet
  int newCursor = atoi(output + 12);

  ReplaceBufferContent(selectedBuffer, nextTextStart);
  // memmove(selectedBuffer->content, nextTextStart, strlen(nextTextStart));
  // selectedBuffer->size = strlen(nextTextStart);
  //
  selectedBuffer->cursor = newCursor;
  VirtualFreeMemory(output);
}
typedef struct Res {
  char filename[256];
  int filenameLen;
  int line;
  int offset;
} Res;

int TryParse(char* line, Res* res) {
  char* p = line;

  if (*p && *p == '.' && *(p + 1) && *(p + 1) == '\\')
    p += 2;

  while (*p && *p >= 'a' && *p <= 'z') {
    res->filename[res->filenameLen++] = *p;
    p++;
  }

  if (*p != '.')
    return 0;

  p++;

  if (*p != 'c')
    return 0;

  p++;

  if (*p != ':')
    return 0;

  p++;

  res->filename[res->filenameLen++] = '.';
  res->filename[res->filenameLen++] = 'c';

  int val = 0;
  if (*p < '0' || *p > '9')
    return 0; // expect digit
  while (*p >= '0' && *p <= '9') {
    val = val * 10 + (*p - '0');
    p++;
  }
  res->line = val;

  if (*p != ':')
    return 1;

  p++;

  val = 0;
  if (*p < '0' || *p > '9')
    return 0; // expect digit
  while (*p >= '0' && *p <= '9') {
    val = val * 10 + (*p - '0');
    p++;
  }
  res->offset = val;

  return 1;
}

Res res[10];
void Parse() {
  int isStartOfLine = 1;
  int line = 0;
  int errorCount = 0;
  Buffer* buf = &compilationOutputBuffer;
  for (int i = 0; i < buf->size; i++) {
    char ch = buf->content[i];
    if (isStartOfLine && errorCount < 9) {
      Res r = {0};
      if (TryParse(buf->content + i, &r))
        res[errorCount++] = r;

      isStartOfLine = 0;
    }
    //
    if (ch == '\n') {
      isStartOfLine = 1;
      line++;
    }
  }
}

void NavigateToError(int pos) {
  Res r = res[pos];
  char fullPath[256] = {0};
  sprintf(fullPath, "%s%s", rootDir, r.filename);

  if (!IsStrEqual(selectedBuffer->filePath, fullPath))
    LoadFileIntoSelectedBuffer(fullPath);

  int desiredLine = r.line;
  int line = 1;
  if (desiredLine == 0)
    SetCursorPosition(0);
  for (int i = 0; i < selectedBuffer->size; i++) {
    if (selectedBuffer->content[i] == '\n') {
      line++;
      if (line == desiredLine) {
        SetCursorPosition(i + r.offset);

        break;
      }
    }
  }
}

void RunCode() {
  SaveSelectedFile();
  char* cmd = "cmd /c .\\misc\\build.bat";
  int len = 0;
  char output[KB(20)];
  RunCommand(cmd, output, &len);
  CopyStrIntoBuffer(&compilationOutputBuffer, output, len);
  OnLayout();
  Parse();
}

void BuildLib() {
  SaveSelectedFile();
  char* cmd = "cmd /c .\\misc\\build.bat";
  int len = 0;
  char output[KB(20)];

  char* s = "Hello there\n";
  memmove(output, s, strlen(s));
  len = strlen(s);
  // RunCommand(cmd, output, &len);
  CopyStrIntoBuffer(&compilationOutputBuffer, output, len);
  OnLayout();
  Parse();
}

void MoveToMatchingCharacter() {
  int matchingIndex = GetMatchingCharIndex();
  if (matchingIndex >= 0)
    SetCursorPosition(matchingIndex);
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
      if (currentCommand[i].ch != str[i])
        return 0;
    }
    hasMatchedAnyCommand = 1;
    return 1;
  }
  return 0;
}

i32 IsCharCommand(char ch) {
  if (currentCommandLen == 1 && currentCommand[0].ch == ch) {
    hasMatchedAnyCommand = 1;
    return 1;
  }
  return 0;
}

i32 IsCtrlCharCommand(char ch) {
  if (currentCommandLen == 1 && currentCommand[0].ch == ch && currentCommand[0].ctrl) {
    hasMatchedAnyCommand = 1;
    return 1;
  }
  return 0;
}

i32 IsAltCharCommand(char ch) {
  if (currentCommandLen == 1 && currentCommand[0].ch == ch && currentCommand[0].alt) {
    hasMatchedAnyCommand = 1;
    return 1;
  }
  return 0;
}
void ClearCommand() {
  visibleCommandLen = currentCommandLen;
  currentCommandLen = 0;
}

int isPlayingLastRepeat = 0;
void SaveCommand() {
  if (!isPlayingLastRepeat) {
    memmove(lastCommand, currentCommand, currentCommandLen * sizeof(Key));
    lastCommandLen = currentCommandLen;
  }
}
void MoveTextRight() {
  if (mode == Visual || mode == VisualLine) {
    SelectionRange range = GetSelectionRange();
    int pos = FindLineStart(selectedBuffer, range.left);
    int charsAdded = 0;
    while (pos < range.right) {
      InsertCharAt(selectedBuffer, pos, ' ');
      InsertCharAt(selectedBuffer, pos, ' ');
      charsAdded += 2;
      pos = FindLineEnd(selectedBuffer, pos) + 1;
    }
    if (selectedBuffer->selectionStart < selectedBuffer->cursor)
      selectedBuffer->cursor += charsAdded;
    else
      selectedBuffer->selectionStart += charsAdded;
  } else {
    int start = FindLineStart(selectedBuffer, selectedBuffer->cursor);
    InsertCharAt(selectedBuffer, start, ' ');
    InsertCharAt(selectedBuffer, start, ' ');
    SetCursorPosition(selectedBuffer->cursor + 2);
  }
}

void MoveTextLeft() {
  char* text = selectedBuffer->content;
  if (mode == Visual || mode == VisualLine) {
    SelectionRange range = GetSelectionRange();
    int pos = FindLineStart(selectedBuffer, range.left);
    int charsRemoved = 0;
    int charsRemovedOnLine = 0;
    while (pos < range.right) {
      while (text[pos] == ' ' && charsRemovedOnLine < 2) {
        RemoveChar(selectedBuffer, pos);
        charsRemoved += 1;
        charsRemovedOnLine += 1;
      }
      charsRemovedOnLine = 0;
      pos = FindLineEnd(selectedBuffer, pos) + 1;
    }
    if (selectedBuffer->selectionStart < selectedBuffer->cursor)
      selectedBuffer->cursor -= charsRemoved;
    else
      selectedBuffer->selectionStart -= charsRemoved;

  } else {
    int start = FindLineStart(selectedBuffer, selectedBuffer->cursor);
    int charsRemoved = 0;
    while (text[start] == ' ' && charsRemoved < 2) {
      charsRemoved++;
      RemoveChar(selectedBuffer, start);
    }
    SetCursorPosition(selectedBuffer->cursor - charsRemoved);
  }
}

int StrIndexOf(char* text, int size, char* substr) {
  int pos = 0;
  int matchedChar = 0;
  int len = strlen(substr);

  while (pos < size) {
    while (text[pos + matchedChar] == substr[matchedChar]) {
      matchedChar++;
      if (matchedChar == len) {
        return pos;
      }
    }
    matchedChar = 0;
    pos++;
  }
  return -1;
}

void NavigateToFindSearchResult() {
  CtagEntry* entry = found[0];
  if (entry && *entry->filename != '\0') {
    char path[512] = {0};
    sprintf(path, "%s%s", rootDir, entry->filename);

    // todo; check if other split has this filename loaded
    if (!IsStrEqual(path, selectedBuffer->filePath)) {
      LoadFileIntoSelectedBuffer(path);
    }
    int i = StrIndexOf(selectedBuffer->content, selectedBuffer->size, entry->pattern);
    if (i >= 0)
      SetCursorPosition(i);
  }
}

void HandleTagSearchCommand() {
  hasMatchedAnyCommand = 1;
  char ch = currentCommand[0].ch;
  if (ch == VK_BACK) {
    tagsSearchLen = MaxI32(tagsSearchLen - 1, 0);
    tagsSearch[tagsSearchLen] = '\0';
  } else if (ch == VK_RETURN) {
    NavigateToFindSearchResult();
    mode = Normal;
    isTagsSearchVisible = 0;
    hasMatchedAnyCommand = 1;
  } else
    tagsSearch[tagsSearchLen++] = ch;
}
void HandleLocalSearchCommand() {
  Key key = currentCommand[currentCommandLen];
  char ch = key.ch;

  if (IsCharCommand(VK_BACK)) {
    if (searchLen > 0) {
      searchTerm[--searchLen] = '\0';
      FindEntries(selectedBuffer);
    }
  }
  if (ch == 'W' && key.ctrl) {
    searchLen = MaxI32(searchLen - 1, 0);
    while (searchTerm[searchLen] != ' ' || searchLen == 0)
      searchLen--;
    searchTerm[searchLen] = '\0';
    FindEntries(selectedBuffer);
  }
  if (ch == 'U' && key.ctrl) {
    searchLen = 0;
    searchTerm[searchLen] = '\0';
    FindEntries(selectedBuffer);
  }
  if (ch == 'N' && key.ctrl) {
    MoveNextOnSearch();
  }

  if (ch == 'P' && key.ctrl) {
    MovePrevOnSearch();
  }
}

void HandleFileSelectionCommand() {
  char firstChar = currentCommand[0].ch;
  if (firstChar >= '1' && firstChar <= '9') {
    char path[512] = {0};
    sprintf(path, "%s%s", rootDir, allFiles[firstChar - '1']);
    LoadFileIntoSelectedBuffer(path);
    hasMatchedAnyCommand = 1;
  }
}

void HandleInsertCommand() {
  Key key = currentCommand[0];
  char ch = key.ch;
  if (ch == VK_TAB && key.shift)
    MoveTextLeft();
  else if (ch == VK_TAB)
    MoveTextRight();
  else if (ch == 'W' && key.ctrl) {
    int pos = MaxI32(selectedBuffer->cursor - 2, 0);
    char* text = selectedBuffer->content;

    while (pos > 0 && text[pos] != ' ' && text[pos] != '\n')
      pos--;
    if (text[pos] == '\n')
      pos++;
    if (pos != (selectedBuffer->cursor - 1)) {
      RemoveChars(selectedBuffer, pos, selectedBuffer->cursor - 1);
      SetCursorPosition(pos);
      FindEntries(selectedBuffer);

      if (!isPlayingLastRepeat) {
        lastCommand[lastCommandLen].ctrl = 1;
        lastCommand[lastCommandLen++].ch = ch;
      }
    }

    hasMatchedAnyCommand = 1;
  }

  else if (key.ch == 'B' && key.ctrl) {
    RemoveCurrentChar();
    hasMatchedAnyCommand = 1;
  } else if (IsCharCommand(VK_RETURN))
    BreakLineAtCursor(selectedBuffer);

  else if (IsCtrlCharCommand('R')) {
    Change* appliedChange = RedoLastChange(selectedBuffer);
    if (appliedChange && appliedChange->type != Replaced)
      SetCursorPosition(appliedChange->at);
  }
  if (IsCtrlCharCommand('D')) {
    selectedOffset->target += (float)selectedRect->height / 2.0f;
  } else if (IsCtrlCharCommand('U')) {
    selectedOffset->target -= (float)selectedRect->height / 2.0f;
  } else if (ch < MAX_CHAR_CODE && ch >= ' ' && !key.ctrl && !key.alt) {
    InsertCharAtCursor(selectedBuffer, ch);

    hasMatchedAnyCommand = 1;
    if (!isPlayingLastRepeat)
      lastCommand[lastCommandLen++].ch = ch;
    if (isSearchVisible)
      FindEntries(selectedBuffer);
  }
}

void HandleVisualAndNormalCommands() {
  char ch = currentCommand[0].ch;
  if (ch == VK_RETURN) {
    BreakLineAtCursor(selectedBuffer);
    hasMatchedAnyCommand = 1;
  } else if (ch == VK_BACK) {
    RemoveCurrentChar();
    hasMatchedAnyCommand = 1;
  }
  if (IsCommand("j"))
    MoveDown();

  if (IsCommand("%"))
    MoveToMatchingCharacter();

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

  if (currentCommandLen == 2 && currentCommand[0].ch == 'f') {
    SetCursorPosition(
        FindNextChar(selectedBuffer, selectedBuffer->cursor + 1, currentCommand[1].ch));
    hasMatchedAnyCommand = 1;
  }

  if (currentCommandLen == 2 && currentCommand[0].ch == 't') {
    SetCursorPosition(
        FindNextChar(selectedBuffer, selectedBuffer->cursor + 1, currentCommand[1].ch) - 1);
    hasMatchedAnyCommand = 1;
  }

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

  if (IsCommand("zz"))
    CenterViewOnCursor();
}

void HandleVisualCommand() {
  Key key = currentCommand[0];
  char ch = key.ch;
  SelectionRange range = GetSelectionRange();
  if (IsCommand("d")) {
    RemoveChars(selectedBuffer, range.left, range.right);
    selectedBuffer->cursor = MinI32(selectedBuffer->cursor, selectedBuffer->selectionStart);
    mode = Normal;
  }
  if (IsCommand("o")) {
    i32 temp = selectedBuffer->cursor;
    selectedBuffer->cursor = selectedBuffer->selectionStart;
    selectedBuffer->selectionStart = temp;
  }
  if (IsCommand("y")) {
    ClipboardCopy(mainWindow, selectedBuffer->content + range.left, range.right - range.left + 1);
    // mode = Normal;
  }

  if (ch == VK_TAB && key.shift) {
    MoveTextLeft();
    hasMatchedAnyCommand = 1;
  } else if (ch == VK_TAB) {
    MoveTextRight();
    hasMatchedAnyCommand = 1;
  }
}

void HandleNormalCommand() {
  Key key = currentCommand[0];
  char ch = key.ch;
  if (IsCommand("r"))
    mode = ReplaceChar;
  else if (ch == VK_TAB && key.shift) {
    MoveTextLeft();
    hasMatchedAnyCommand = 1;
  } else if (ch == VK_TAB) {
    MoveTextRight();
    hasMatchedAnyCommand = 1;
  }
  if (IsCommand(" m")) {
    int len = 0;
    char output[KB(20)];
    RunCommand("ctags --fields=+ne *", output, &len);

    ReadCtagsFile();
    isTagsSearchVisible = !isTagsSearchVisible;
  }
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
    SaveCommand();
  }
  if (IsCommand("dw")) {
    int from = selectedBuffer->cursor;
    int lineEnd = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
    int to = MinI32(JumpWordForward(selectedBuffer) - 1, lineEnd - 1);
    RemoveChars(selectedBuffer, from, to);
    SaveCommand();
  }
  if (IsCommand("cw") || IsCommand("cW")) {
    int from = selectedBuffer->cursor;
    int lineEnd = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
    int to = MinI32(JumpWordForward(selectedBuffer) - 1, lineEnd - 1);
    if (currentCommand[1].ch == 'W')
      to = MinI32(JumpWordWithPunctuationForward(selectedBuffer) - 1, lineEnd - 1);

    if (selectedBuffer->content[to] == ' ')
      to--;
    RemoveChars(selectedBuffer, from, to);
    mode = Insert;
    SaveCommand();
    FindEntries(selectedBuffer);
  }

  if (IsCtrlCharCommand('J')) {
    int lineStart = FindLineStart(selectedBuffer, selectedBuffer->cursor);
    int cursorOffset = selectedBuffer->cursor - lineStart;
    int currentLineOffset = GetLineOffset(selectedBuffer, lineStart);

    SetCursorPosition(selectedBuffer->cursor + 5);
  }

  if (IsCtrlCharCommand('K')) {
    SetCursorPosition(selectedBuffer->cursor - 5);
  }
  // TODO: these are starting to look like patterns for operation/motion common code
  if (currentCommandLen > 2) {
    char op = currentCommand[0].ch;
    char motion = currentCommand[1].ch;

    int isValidOp = (op == 'd' || op == 'c' || op == 'y');
    int isValidMotion = motion == 'f' || motion == 't';

    if (!hasMatchedAnyCommand && currentCommandLen == 3 && isValidOp && isValidMotion) {
      char ch = currentCommand[2].ch;
      int from = selectedBuffer->cursor;
      int to = 0;
      if (motion == 'f')
        to = FindNextChar(selectedBuffer, selectedBuffer->cursor + 1, ch);

      if (motion == 't')
        to = FindNextChar(selectedBuffer, selectedBuffer->cursor + 1, ch) - 1;

      if (op == 'd')
        RemoveChars(selectedBuffer, from, to);
      if (op == 'c') {
        RemoveChars(selectedBuffer, from, to);
        mode = Insert;
      }
      if (op == 'y')
        ClipboardCopy(mainWindow, selectedBuffer->content + from, to - from + 1);

      SaveCommand();
      hasMatchedAnyCommand = 1;
    }
  }

  if (IsCommand("O")) {
    i32 lineStart = FindLineStart(selectedBuffer, selectedBuffer->cursor);
    i32 offset = GetLineOffset(selectedBuffer, lineStart);

    InsertCharAt(selectedBuffer, lineStart, '\n');
    for (int i = 0; i < offset; i++)
      InsertCharAt(selectedBuffer, lineStart + i, ' ');

    selectedBuffer->cursor = lineStart + offset;
    mode = Insert;
  }
  if (IsCommand("o")) {
    i32 lineEnd = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
    i32 offset =
        GetLineOffset(selectedBuffer, FindLineStart(selectedBuffer, selectedBuffer->cursor));

    InsertCharAt(selectedBuffer, lineEnd, '\n');
    for (int i = 0; i < offset; i++)
      InsertCharAt(selectedBuffer, lineEnd + 1 + i, ' ');

    selectedBuffer->cursor = lineEnd + 1 + offset;
    mode = Insert;
  }
  if (IsCommand("v")) {
    mode = Visual;
    selectedBuffer->selectionStart = selectedBuffer->cursor;
  }
  if (IsCommand("V")) {
    mode = VisualLine;
    selectedBuffer->selectionStart = selectedBuffer->cursor;
  }
  if (IsCommand("A")) {
    selectedBuffer->cursor = FindLineEnd(selectedBuffer, selectedBuffer->cursor);
    mode = Insert;
  }
  if (IsCommand("i")) {
    mode = Insert;
    SaveCommand();
  }
  if (IsCommand("a")) {
    selectedBuffer->cursor = MaxI32(selectedBuffer->cursor - 1, 0);
    mode = Insert;
    SaveCommand();
  }
  if (IsCommand("I")) {
    selectedBuffer->cursor = FindLineStart(selectedBuffer, selectedBuffer->cursor);
    while (selectedBuffer->cursor < selectedBuffer->size &&
           IsWhitespace(selectedBuffer->content[selectedBuffer->cursor]))
      selectedBuffer->cursor++;

    mode = Insert;
    SaveCommand();
  }
  if (IsCommand("C")) {
    i32 start = FindLineStart(selectedBuffer, selectedBuffer->cursor);
    i32 end = FindLineEnd(selectedBuffer, selectedBuffer->cursor) - 1;
    if (start != end) {
      RemoveChars(selectedBuffer, selectedBuffer->cursor, end);
    }
    mode = Insert;
    SaveCommand();
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

    int whereToInsert = selectedBuffer->cursor + 1;
    int isInsertingNewLine = StrContainsChar(textFromClipboard, '\n');
    if (isInsertingNewLine)
      whereToInsert = FindLineEnd(selectedBuffer, selectedBuffer->cursor) + 1;

    InsertChars(selectedBuffer, textFromClipboard, size, whereToInsert);
    if (isInsertingNewLine)
      selectedBuffer->cursor = whereToInsert;
    else
      selectedBuffer->cursor = whereToInsert + size;

    if (textFromClipboard)
      VirtualFreeMemory(textFromClipboard);
  }

  if (IsCommand("P")) {
    i32 size = 0;
    char* textFromClipboard = ClipboardPaste(mainWindow, &size);

    int whereToInsert = selectedBuffer->cursor;
    int isInsertingNewLine = StrContainsChar(textFromClipboard, '\n');
    if (isInsertingNewLine)
      whereToInsert = FindLineStart(selectedBuffer, selectedBuffer->cursor);

    InsertChars(selectedBuffer, textFromClipboard, size, whereToInsert);

    if (isInsertingNewLine)
      selectedBuffer->cursor = whereToInsert;
    else
      selectedBuffer->cursor = whereToInsert + size;

    if (textFromClipboard)
      VirtualFreeMemory(textFromClipboard);
  }

  if (currentCommandLen == 2 && currentCommand[0].ch == 'e') {
    if (currentCommand[1].ch >= '0' && currentCommand[1].ch <= '9') {
      NavigateToError(currentCommand[1].ch - '1');
      hasMatchedAnyCommand = 1;
    }
  }

  if (IsAltCharCommand('1'))
    SelectFile(Left);
  if (IsAltCharCommand('2'))
    SelectFile(Middle);
  if (IsAltCharCommand('3'))
    SelectFile(Right);
  if (IsCtrlCharCommand('S'))
    SaveSelectedFile();
  if (IsCtrlCharCommand(VK_OEM_PLUS)) {
    fontSize += 1;
    fontArena.bytesAllocated = 0;
    ResetAppFonts();
  }
  if (IsCtrlCharCommand(VK_OEM_MINUS)) {
    fontSize -= 1;
    fontArena.bytesAllocated = 0;
    ResetAppFonts();
  }
  if (IsCommand(" r"))
    RunCode();

  if (IsCommand(" l"))
    BuildLib();
  if (IsCommand(" e"))
    mode = FileSelection;

  if (IsCommand(" f"))
    FormatSelectedFile();

  if (IsCommand(" y")) {
    isFullscreen = !isFullscreen;
    SetFullscreen(mainWindow, isFullscreen);
  }

  if (IsCommand(" w"))
    SaveSelectedFile();

  if (IsCommand("u")) {
    i32 currentCursor = selectedBuffer->cursor;
    Change* appliedChange = UndoLastChange(selectedBuffer);
    if (appliedChange && appliedChange->type != Replaced)
      SetCursorPosition(appliedChange->at);
  }

  if (IsCommand(" n")) {
    isSearchVisible = 0;
  }

  if (IsCommand("n")) {
    if (!isSearchVisible) {
      isSearchVisible = 1;
      StartSearch();
    } else {
      MoveNextOnSearch();
    }
  }

  if (IsCommand("N")) {
    currentEntry--;
    if (currentEntry < 0)
      currentEntry = entriesCount - 1;
    SetCursorPosition(entriesAt[currentEntry].at);
  }

  if (IsCommand("/")) {
    mode = LocalSearchTyping;

    isSearchVisible = 1;
  }

  if (IsCommand(" q")) {
    PostQuitMessage(0);
    isRunning = 0;
  }
}

void AppendCharIntoCommand(Key key) {
  currentCommand[currentCommandLen++] = key;
  char ch = key.ch;
  visibleCommandLen = 0;
  hasMatchedAnyCommand = 0;
  if (key.ch == VK_ESCAPE) {
    mode = Normal;
    isTagsSearchVisible = 0;
    hasMatchedAnyCommand = 1;
    if (!isPlayingLastRepeat)
      lastCommand[lastCommandLen++].ch = ch;
  } else if (isTagsSearchVisible)
    HandleTagSearchCommand();
  else if (mode == LocalSearchTyping)
    HandleLocalSearchCommand();
  else if (mode == FileSelection)
    HandleFileSelectionCommand();
  else if (mode == Insert)
    HandleInsertCommand();
  else {
    HandleVisualAndNormalCommands();
    if (mode == Visual || mode == VisualLine)
      HandleVisualCommand();
    else if (mode == Normal)
      HandleNormalCommand();
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
      if (mode == Normal && wParam == '.' && currentCommandLen == 0) {
        isPlayingLastRepeat = 1;
        for (int i = 0; i < lastCommandLen; i++) {
          AppendCharIntoCommand(lastCommand[i]);
        }
        isPlayingLastRepeat = 0;
      } else if (mode == LocalSearchTyping) {
        searchTerm[searchLen++] = wParam;
        StartSearch();
      } else if (mode == ReplaceChar) {
        selectedBuffer->content[selectedBuffer->cursor] = ch;
        mode = Normal;
      } else
        AppendCharIntoCommand((Key){.ch = ch});
    }
    break;
  case WM_SYSKEYDOWN:
  case WM_KEYDOWN:
    if ((IsKeyPressed(VK_CONTROL) && wParam != VK_CONTROL) ||
        (IsKeyPressed(VK_MENU) && wParam != VK_MENU) || wParam == VK_ESCAPE ||
        wParam == VK_RETURN || wParam == VK_TAB || wParam == VK_BACK)
      AppendCharIntoCommand((Key){.ch = wParam,
                                  .shift = IsKeyPressed(VK_SHIFT),
                                  .alt = IsKeyPressed(VK_MENU),
                                  .ctrl = IsKeyPressed(VK_CONTROL)});
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

    OnLayout();
  }
  return DefWindowProc(window, message, wParam, lParam);
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
  int commandX = (strlen(selectedBuffer->filePath) + 2) * font.charWidth;
  for (int i = 0; i < charsToShow; i++) {
    u32 color = currentCommandLen > 0 ? 0xffffff : 0xbbbbbb;
    CopyMonochromeTextureRectTo(&canvas, &footerRect, &font.textures[currentCommand[i].ch],
                                commandX, y, color);
    commandX += font.charWidth;
  }
  char posLabel[512];

  CursorPos cursor = GetCursorPosition(selectedBuffer);
  char lastCommandUI[512] = {0};
  for (int i = 0; i < lastCommandLen; i++)
    lastCommandUI[i] = lastCommand[i].ch;

  // memmkve(lastCommandUI, lastCommand, lastCommandLen);
  int chars =
      sprintf(posLabel,
              "%s    changes size %d, capacity %d   buffer size %d, capacity %d   pos %d, line %d, "
              "offset %d",
              lastCommandUI, GetChangeArenaSize(&selectedBuffer->changeArena),
              selectedBuffer->changeArena.capacity, selectedBuffer->size, selectedBuffer->capacity,
              selectedBuffer->cursor, cursor.line, cursor.lineOffset);
  int width = chars * font.charWidth;
  int posX = screen.width - width - 2 * font.charWidth;
  int posY = footerRect.y + footerPadding;
  for (int i = 0; i < chars; i++) {
    CopyMonochromeTextureRectTo(&canvas, &footerRect, &font.textures[posLabel[i]], posX, posY,
                                0x888888);
    posX += font.charWidth;
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
  if (mode == Visual || mode == VisualLine) {
    SelectionRange range = GetSelectionRange();

    CursorPos startPos = GetPositionOffset(buffer, range.left);
    CursorPos endPos = GetPositionOffset(buffer, range.right);

    i32 len = GetLineLength(selectedBuffer, startPos.line);
    i32 maxLen = len - startPos.lineOffset;
    i32 firstLineLen = MinI32(range.right - range.left, maxLen);

    PaintRect(x + startPos.lineOffset * font.charWidth, y + startPos.line * lineHeightPx,
              firstLineLen * font.charWidth, lineHeightPx, bg);

    if (startPos.line != endPos.line) {
      for (i32 l = startPos.line + 1; l < endPos.line; l++) {
        PaintRect(x, y + l * lineHeightPx, GetLineLength(selectedBuffer, l) * font.charWidth,
                  lineHeightPx, bg);
      }

      PaintRect(x, y + endPos.line * lineHeightPx, (endPos.lineOffset + 1) * font.charWidth,
                lineHeightPx, bg);
    }
  }
}

void DrawArea(Rect rect, Buffer* buffer, Spring* offset, EdFile file) {
  if (rect.width == 0)
    return;
  i32 startX = rect.x + horizPadding;
  i32 startY = vertPadding - (i32)offset->current;
  i32 x = startX;
  i32 y = rect.y + startY;

  u32 cursorColor = mode == Normal                           ? colorsCursorNormal
                    : mode == LocalSearchTyping              ? colorsCursorLocalSearchType
                    : (mode == Visual || mode == VisualLine) ? colorsCursorVisual
                                                             : colorsCursorInsert;

  CursorPos cursor = GetCursorPosition(buffer);
  i32 cursorX = x + font.charWidth * cursor.lineOffset;
  i32 cursorY = y + lineHeightPx * cursor.line;

  u32 lineColor = mode == Normal                           ? colorsCursorLineNormal
                  : mode == LocalSearchTyping              ? colorsCursorLineLocalSearchType
                  : (mode == Visual || mode == VisualLine) ? colorsCursorLineVisual
                                                           : colorsCursorLineInsert;

  if (!buffer->isSaved)
    PaintRect(rect.x + rect.width - 30 - scrollbarWidth, rect.y, 20, 10, 0x882222);

  int hasCursor = file == selectedFile && mode != FileSelection;
  if (hasCursor) {
    // cursor line background
    PaintRect(rect.x, cursorY, rect.width, lineHeightPx, lineColor);
  }

  i32 charShift = (lineHeightPx - font.charHeight) / 2;

  if (selectedBuffer == buffer && isSearchVisible) {
    i32 currentSearchEntryIndex = 0;
    EntryFound* found = NULL;
    i32 charY = rect.y + startY;

    for (i32 i = 0; i < buffer->size; i++) {
      if (currentSearchEntryIndex < entriesCount)
        found = &entriesAt[currentSearchEntryIndex];
      else
        found = NULL;

      if (found && i >= found->at && i < found->at + found->len) {
        PaintRect(x, charY, font.charWidth, lineHeightPx, colorsSearchResult);
      }

      char ch = buffer->content[i];
      if (ch == '\n') {
        x = startX;
        charY += lineHeightPx;
      } else {
        x += font.charWidth;
      }

      if (found && i >= found->at + found->len) {
        currentSearchEntryIndex++;
      }
    }

    int searchX = rect.x + rect.width - 400;
    int searchY = rect.y + 13;
    for (i32 i = 0; i < searchLen; i++) {
      u32 searchColor = mode == LocalSearchTyping ? 0x66ffff : 0xaaaaaa;
      CopyMonochromeTextureRectTo(&canvas, &rect, &font.textures[searchTerm[i]], searchX, searchY,
                                  searchColor);
      searchX += font.charWidth;
    }
    char s[512] = {0};
    int visibleCurrent = entriesCount > 0 ? currentEntry + 1 : 0;
    sprintf(s, "%d of %d", visibleCurrent, entriesCount);
    char* a = s;
    searchX += font.charWidth * 4;
    while (*a) {
      CopyMonochromeTextureRectTo(&canvas, &rect, &font.textures[*a], searchX, searchY, 0xaaaaaa);
      searchX += font.charWidth;
      a++;
    }
  }

  if (hasCursor) {
    // selection
    if (selectedFile == file)
      DrawSelection(buffer, rect, offset);

    // cursor
    PaintRect(cursorX, cursorY, font.charWidth, lineHeightPx, cursorColor);
  }

  x = startX;
  y = rect.y + startY;

  u32 colorKeyword = 0x359ED2;
  u32 colorCode = 0xA0DAFB;
  u32 colorPreprocessor = 0xC686BD;
  u32 colorType = 0x46C8B1;
  u32 colorComment = 0x666666;
  u32 colorString = 0xCC937B;
  int isTokenized = strlen(buffer->filePath) > 0 && StrEndsWith(buffer->filePath, ".c");
  if (isTokenized)
    SplitIntoTokens(buffer);
  else
    tokensCount = 0;

  int currentToken = 0;

  for (i32 i = 0; i < buffer->size; i++) {
    i32 charY = y + charShift;
    char ch = buffer->content[i];
    Token* t =
        (currentToken < tokensCount && tokens[currentToken].start <= i) ? &tokens[currentToken] : 0;
    u32 color = colorsFont;
    if (t && t->type == Preprocessor)
      color = colorPreprocessor;
    if (t && t->type == Keyword)
      color = colorKeyword;
    if (t && t->type == Type)
      color = colorType;
    if (t && t->type == Comment)
      color = colorComment;
    if (t && (t->type == StringLiteral || t->type == CharLiteral))
      color = colorString;

    if (ch == '\n') {
      x = startX;
      y += lineHeightPx;
    } else if (ch < MAX_CHAR_CODE && ch >= ' ') {

      u32 charColor = (i == buffer->cursor && hasCursor) ? colorsBg : color;
      CopyMonochromeTextureRectTo(&canvas, &rect, &font.textures[ch], x, charY, charColor);

      if (buffer == selectedBuffer && i == buffer->oposingCharAt)
        PaintRect(x, charY + lineHeightPx - 3, font.charWidth, 3, cursorColor);

      x += font.charWidth;
    } else {
      PaintRect(x, charY, font.charWidth, lineHeightPx, 0xff0000);
      x += font.charWidth;
    }

    if (t && i >= (t->start + t->len - 1))
      currentToken++;
  }

  if (mode == FileSelection && buffer == selectedBuffer) {
    i32 fileBoxWidth = 400;
    int boxX = rect.x + rect.width / 2 - fileBoxWidth / 2;
    int boxY = rect.y + 20;
    PaintRect(boxX, boxY, fileBoxWidth, 600, 0x222222);
    int boxVpadding = 5;
    int boxHpadding = 10;

    boxY += boxVpadding;
    boxX += boxHpadding;

    for (i32 i = 0; i < ArrayLength(allFiles); i++) {
      char* label = allFiles[i];
      int textX = boxX;
      CopyMonochromeTextureRectTo(&canvas, &rect, &font.textures[i + '1'], textX, boxY, 0x888888);
      textX += font.charWidth * 2;
      while (*label) {
        CopyMonochromeTextureRectTo(&canvas, &rect, &font.textures[*label], textX, boxY, 0xffffff);
        label++;
        textX += font.charWidth;
      }
      boxY += font.charHeight * 1.2;
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

  RectFillRightBorder(compilationRect, 4, borderColor);
  RectFillTopBorder(compilationRect, 4, borderColor);

  DrawArea(leftRect, &leftBuffer, &leftOffset, Left);
  DrawArea(middleRect, &middleBuffer, &middleOffset, Middle);
  DrawArea(rightRect, &rightBuffer, &rightOffset, Right);
  DrawArea(compilationRect, &compilationOutputBuffer, &compilationOffset, CompilationResults);

  DrawFooter();
  if (isTagsSearchVisible)
    DrawTagsSearch(&font);

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

  fontArena = CreateArena(500 * 1024);
  InitFont(&font, fontName, fontSize, &fontArena);

  mainWindow = OpenWindow(OnEvent, colorsBg, "Editor");

  if (isFullscreen)
    SetFullscreen(mainWindow, 1);

  dc = GetDC(mainWindow);

  leftBuffer = ReadFileIntoDoubledSizedBuffer(leftFilePath);
  InitChangeArena(&leftBuffer);

  middleBuffer = ReadFileIntoDoubledSizedBuffer(middleFilePath);
  InitChangeArena(&middleBuffer);

  rightBuffer = ReadFileIntoDoubledSizedBuffer(rightFilePath);
  InitChangeArena(&rightBuffer);

  compilationOutputBuffer.capacity = KB(500);
  compilationOutputBuffer.content = VirtualAllocateMemory(compilationOutputBuffer.capacity);

  SelectFile(Middle);

  InitAnimations();

  tokens = VirtualAllocateMemory(sizeof(Token) * 20000000);
  // InitChanges(selectedBuffer, &changeArena);
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
