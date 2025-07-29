#define WIN32_LEAN_AND_MEAN
#include <dwmapi.h>
#include <stdint.h>
#include <windows.h>

// config
bool isCaseSensitiveSearch = false;
//

typedef struct Spring {
  float target;
  float velocity;
  float current;
} Spring;

float stiffness;
float damping;

void InitAnimations() {
  stiffness = 600;
  damping = 1.7 * 25.0f;
}

void UpdateSpring(Spring* spring, float deltaSec) {
  float displacement = spring->target - spring->current;
  float springForce = displacement * stiffness;
  float dampingForce = spring->velocity * damping;

  spring->velocity += (springForce - dampingForce) * deltaSec;
  spring->current += spring->velocity * deltaSec;
}

extern "C" int _fltused = 0x9875;

int isRunning = 1;
int isF = 0;

typedef int64_t i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef int32_t i32;
typedef float f32;
typedef wchar_t wc;

enum Mode { Normal, Insert, SearchInput };
enum SearchDirection { down, up };
char searchTerm[255];
i32 searchTermLen;
SearchDirection searchDirection;

typedef struct MyBitmap {
  i32 width;
  i32 height;
  i32 bytesPerPixel;
  u32* pixels;
} MyBitmap;

BITMAPINFO bitmapInfo;
MyBitmap canvas;
HBITMAP canvasBitmap;

f32 padding = 20.0f;
f32 lineHeight = 1.1f;
i32 pos = 0;
i32 desiredOffset = 0;
HDC dc;
HWND win;
Mode mode = Normal;
char* content;
i64 size;
i64 capacity;
Spring offset;
bool isSaved = true;
TEXTMETRICA m;
const char* path = "progress.txt";

#define ArrayLength(array) (sizeof(array) / sizeof(array[0]))

inline void* valloc(size_t size) {
  return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
};

inline void vfree(void* ptr) {
  VirtualFree(ptr, 0, MEM_RELEASE);
};

// Increasing Read Bandwidth with SIMD Instructions
// https://www.computerenhance.com/p/increasing-read-bandwidth-with-simd
#pragma function(memset)
void* memset(void* dest, int c, size_t count) {
  char* bytes = (char*)dest;
  while (count--) {
    *bytes++ = (char)c;
  }
  return dest;
}

#pragma function(strlen)
size_t strlen(const char* str) {
  i32 res = 0;
  while (str[res] != '\0')
    res++;
  return res;
}

bool strequal(const char* a, const char* b) {
  while (*a && *b) {
    if (*a != *b)
      return false;
    a++;
    b++;
  }
  return *a == *b;
}

// TODO: no need to allocate memory each call, pre-allocate memory and check it's capacity
#pragma function(memmove)
void myMemMove(void* dest, void* src, size_t n) {
  char* csrc = (char*)src;
  char* cdest = (char*)dest;

  // Create a temporary array to hold data of src
  char* temp = (char*)valloc(n);

  // Copy data from csrc[] to temp[]
  for (int i = 0; i < n; i++)
    temp[i] = csrc[i];

  // Copy data from temp[] to cdest[]
  for (int i = 0; i < n; i++)
    cdest[i] = temp[i];

  vfree(temp);
}

extern "C" void* memcpy(void* dst, const void* src, size_t n) {
  unsigned char* d = (unsigned char*)dst;
  const unsigned char* s = (const unsigned char*)src;
  while (n--)
    *d++ = *s++;
  return dst;
}

typedef BOOL WINAPI set_process_dpi_aware(void);
typedef BOOL WINAPI set_process_dpi_awareness_context(DPI_AWARENESS_CONTEXT);
static void PreventWindowsDPIScaling() {
  HMODULE WinUser = LoadLibraryW(L"user32.dll");
  set_process_dpi_awareness_context* SetProcessDPIAwarenessContext =
      (set_process_dpi_awareness_context*)GetProcAddress(WinUser, "SetProcessDPIAwarenessContext");
  if (SetProcessDPIAwarenessContext) {
    SetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
  } else {
    set_process_dpi_aware* SetProcessDPIAware =
        (set_process_dpi_aware*)GetProcAddress(WinUser, "SetProcessDPIAware");
    if (SetProcessDPIAware) {
      SetProcessDPIAware();
    }
  }
}

inline i64 GetPerfFrequency() {
  LARGE_INTEGER res;
  QueryPerformanceFrequency(&res);
  return res.QuadPart;
}

inline i64 GetPerfCounter() {
  LARGE_INTEGER res;
  QueryPerformanceCounter(&res);
  return res.QuadPart;
}

// https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
WINDOWPLACEMENT prevWindowDimensions = {sizeof(prevWindowDimensions)};
void SetFullscreen(HWND window, int isFullscreen) {
  DWORD style = GetWindowLong(window, GWL_STYLE);
  if (isFullscreen) {
    MONITORINFO monitorInfo = {sizeof(monitorInfo)};
    if (GetWindowPlacement(window, &prevWindowDimensions) &&
        GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo)) {
      SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);

      SetWindowPos(window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                   monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                   monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
  } else {
    SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(window, &prevWindowDimensions);
    SetWindowPos(window, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
  }
}

i64 GetMyFileSize(const char* path) {
  HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  LARGE_INTEGER size = {0};
  GetFileSizeEx(file, &size);

  CloseHandle(file);
  return (i64)size.QuadPart;
}

void ReadFileInto(const char* path, u32 fileSize, char* buffer) {
  HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

  DWORD bytesRead;
  ReadFile(file, buffer, fileSize, &bytesRead, 0);
  CloseHandle(file);
}

void WriteMyFile(const char* path, char* content, int size) {
  HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

  DWORD bytesWritten;
  int res = WriteFile(file, content, size, &bytesWritten, 0);
  CloseHandle(file);
}

i32 round(f32 v) {
  return (i32)(v + 0.5f);
}

i32 Max(i32 a, i32 b) {
  if (a > b)
    return a;
  return b;
}

i32 Min(i32 a, i32 b) {
  if (a < b)
    return a;
  return b;
}

struct CursorPos {
  i32 row;
  i32 col;
};

CursorPos GetCursorPos() {
  CursorPos res = {0};
  i32 rowStartAt = 0;
  for (i32 i = 0; i < pos; i++) {
    if (content[i] == '\n') {
      res.row++;
      rowStartAt = i + 1;
    }
  }
  res.col = pos - rowStartAt;
  return res;
}

i32 GetLinesCount() {
  i32 res = 1;
  for (i32 i = 0; i < size; i++) {
    if (content[i] == '\n') {
      res++;
    }
  }
  return res;
}

void OffsetCenteredOnCursor() {
  f32 lineHeightPx = m.tmHeight * lineHeight;
  CursorPos p = GetCursorPos();
  f32 cursorY = p.row * lineHeightPx + padding;
  offset.target = Max(cursorY - (canvas.height / 2.0f), 0);
}
void ScrollIntoCursor() {
  i32 itemsToLookAhead = 5;
  f32 lineHeightPx = m.tmHeight * lineHeight;

  CursorPos p = GetCursorPos();

  f32 cursorY = p.row * lineHeightPx + padding;

  f32 spaceToLookAhead = lineHeightPx * itemsToLookAhead;

  f32 pageHeight = GetLinesCount() * lineHeightPx + padding * 2;
  if (pageHeight > canvas.height) {
    if ((cursorY + spaceToLookAhead) > (offset.target + canvas.height)) {
      OffsetCenteredOnCursor();
    }
    if ((cursorY - spaceToLookAhead) < offset.target) {
      OffsetCenteredOnCursor();
    }
  } else {
    offset.target = 0;
  }
}

u32 ToWinColor(u32 color) {
  return ((color & 0xff0000) >> 16) | (color & 0x00ff00) | ((color & 0x0000ff) << 16);
}

void TextColors(u32 foreground, u32 background) {
  SetBkColor(dc, ToWinColor(background));
  SetTextColor(dc, ToWinColor(foreground));
}

void InitBitmapInfo(BITMAPINFO* bitmapInfo, i32 width, i32 height) {
  bitmapInfo->bmiHeader.biSize = sizeof(bitmapInfo->bmiHeader);
  bitmapInfo->bmiHeader.biBitCount = 32;
  bitmapInfo->bmiHeader.biWidth = width;
  bitmapInfo->bmiHeader.biHeight = -height; // makes rows go up, instead of going down by default
  bitmapInfo->bmiHeader.biPlanes = 1;
  bitmapInfo->bmiHeader.biCompression = BI_RGB;
}

void PaintRect(f32 x, f32 y, f32 width, f32 height, u32 color) {
  int startX = round(x);
  int startY = round(y);
  int endX = round(x + width);
  int endY = round(y + height);

  startX = Max(startX, 0);
  startY = Max(startY, 0);

  endX = Min(endX, canvas.width);
  endY = Min(endY, canvas.height);

  for (i32 y = startY; y < endY; y++) {
    for (i32 x = startX; x < endX; x++) {
      canvas.pixels[x + y * canvas.width] = color;
    }
  }
}

void Size(LPARAM lParam) {
  canvas.width = LOWORD(lParam);
  canvas.height = HIWORD(lParam);
  InitBitmapInfo(&bitmapInfo, canvas.width, canvas.height);
  if (canvasBitmap)
    DeleteObject(canvasBitmap);

  canvasBitmap = CreateDIBSection(dc, &bitmapInfo, DIB_RGB_COLORS, (void**)&canvas.pixels, 0, 0);
  canvas.bytesPerPixel = 4;

  SelectObject(dc, canvasBitmap);
}

void DoubleCapacityIfFull() {
  char* currentStr = content;
  capacity = (capacity == 0) ? 4 : (capacity * 2);
  content = (char*)valloc(capacity);
  if (currentStr) {
    myMemMove(content, currentStr, size);
    vfree(currentStr);
  }
}

void BufferRemoveChars(int from, int to) {
  int num_to_shift = size - (to + 1);

  myMemMove(content + from, content + to + 1, num_to_shift);

  size -= (to - from + 1);
  isSaved = false;
}

void BufferInsertChars(char* chars, i32 len, i32 at) {
  while (size + len >= capacity)
    DoubleCapacityIfFull();

  if (size != 0) {
    size += len;

    char* from = content + at;
    char* to = content + at + len;
    myMemMove(to, from, size - at);
  } else {
    size = len;
  }

  for (i32 i = at; i < at + len; i++) {
    content[i] = chars[i - at];
  }
  isSaved = false;
}

u32 IsWhitespace(char ch) {
  return ch == ' ' || ch == '\n';
}

u32 IsNumeric(char ch) {
  return (ch >= '0' && ch <= '9');
}

u32 IsAlphaNumeric(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

u32 ToLower(char ch) {
  if (ch >= 'A' && ch <= 'Z')
    return ch + ('a' - 'A');
  return ch;
}

u32 ToUpper(char ch) {
  if (ch >= 'a' && ch <= 'a')
    return ch - ('a' - 'A');
  return ch;
}

u32 IsPunctuation(char ch) {
  // use a lookup table
  const char* punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

  const char* p = punctuation;
  while (*p) {
    if (ch == *p) {
      return 1;
    }
    p++;
  }
  return 0;
}

i32 FindLineStart(i32 p) {
  while (p > 0 && content[p - 1] != '\n')
    p--;

  return p;
}

i32 FindLineEnd(i32 p) {
  while (p < size && content[p] != '\n')
    p++;

  return p;
}

i32 JumpWordForward(i32 p) {
  char* text = content;
  if (IsWhitespace(text[p])) {
    while (p < size && IsWhitespace(text[p]))
      p++;
  } else {
    if (IsAlphaNumeric(text[p])) {
      while (p < size && IsAlphaNumeric(text[p]))
        p++;
    } else {
      while (p < size && IsPunctuation(text[p]))
        p++;
    }
    while (p < size && IsWhitespace(text[p]))
      p++;
  }

  return p;
}

i32 JumpWordBackward(i32 p) {
  char* text = content;
  pos = Max(pos - 1, 0);
  i32 isStartedAtWhitespace = IsWhitespace(text[pos]);

  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  if (IsAlphaNumeric(text[pos])) {
    while (pos > 0 && IsAlphaNumeric(text[pos]))
      pos--;
  } else {
    while (pos > 0 && IsPunctuation(text[pos]))
      pos--;
  }
  if (pos != 0)
    pos++;

  if (!isStartedAtWhitespace) {
    while (pos > 0 && IsWhitespace(text[pos]))
      pos--;
  }

  return pos;
}

i32 GetOffset(i32 p) {
  i32 start = FindLineStart(pos);
  i32 off = start;
  while (content[off] == ' ')
    off++;

  return off - start;
}

i32 ApplyDesiredOffset(i32 pos) {
  i32 lineEnd = FindLineEnd(pos);

  return Min(pos + desiredOffset, lineEnd);
}

void UpdateDesiredOffset() {
  desiredOffset = pos - FindLineStart(pos);
}

void UpdateCursorWithoutDesiredOffset(i32 p) {
  pos = Max(Min(p, size - 1), 0);
  ScrollIntoCursor();
}

void UpdateCursor(i32 p) {
  UpdateCursorWithoutDesiredOffset(p);
  UpdateDesiredOffset();
}

void MoveDown() {
  i32 end = FindLineEnd(pos);
  if (end != (size - 1))
    UpdateCursorWithoutDesiredOffset(ApplyDesiredOffset(end + 1));
}

void MoveUp() {
  i32 prevLineStart = FindLineStart(FindLineStart(pos) - 1);
  if (prevLineStart < 0)
    prevLineStart = 0;

  UpdateCursorWithoutDesiredOffset(ApplyDesiredOffset(prevLineStart));
}

void MoveLeft() {
  UpdateCursor(pos - 1);
  UpdateDesiredOffset();
}

void MoveRight() {
  UpdateCursor(pos + 1);
  UpdateDesiredOffset();
}

inline BOOL IsKeyPressed(u32 code) {
  return (GetKeyState(code) >> 15) & 1;
}

int ignoreNextCharEvent = 0;
void EnterInsert() {
  mode = Insert;
  ignoreNextCharEvent = 1;
}

void InsertNewLineWithOffset(i32 where, i32 offset) {
  char toInsert[255] = {'\n'};
  for (i32 i = 0; i < offset; i++)
    toInsert[i + 1] = ' ';
  BufferInsertChars(toInsert, offset + 1, where);
  if (where == 0)
    UpdateCursor(where + offset);
  else
    UpdateCursor(where + offset + 1);
}

void AddLineAbove() {
  i32 offset = GetOffset(pos);
  i32 where = Max(FindLineStart(pos) - 1, 0);

  InsertNewLineWithOffset(where, offset);

  EnterInsert();
}

void AddLineBelow() {

  i32 offset = GetOffset(pos);
  i32 where = FindLineEnd(pos);

  InsertNewLineWithOffset(where, offset);

  EnterInsert();
}

void AddLineInside() {
  i32 offset = GetOffset(pos);
  i32 where = FindLineEnd(pos);

  InsertNewLineWithOffset(where, offset + 2);

  EnterInsert();
}

struct Key {
  bool ctrl;
  bool alt;
  i64 ch;
};

Key keys[255];
i32 keysLen;

bool isMatch;
bool IsCommand(const char* cmd) {
  i32 len = strlen(cmd);
  if (keysLen == len) {
    for (i32 i = 0; i < keysLen; i++) {
      if (keys[i].ch != cmd[i])
        return false;
    }
    isMatch = true;
    return true;
  }
  return false;
}

i32 pow(i32 b, i32 p) {
  i32 res = 1;
  for (i32 i = 0; i < p; i++)
    res *= b;
  return res;
}

i32 atoi(Key* buff, i32 len) {
  i32 res = 0;
  for (i32 i = 0; i < len; i++) {
    res += pow(buff[i].ch - '0', len - i);
  }
  return res;
}

bool IsComplexCommand(const char* prefix, const char* postfix, i32* count) {
  i32 prefixLen = strlen(prefix);
  i32 postfixLen = strlen(postfix);
  if (keysLen >= (prefixLen + postfixLen)) {
    for (i32 i = 0; i < prefixLen; i++) {
      if (keys[i].ch != prefix[i])
        return false;
    }
    for (i32 i = 0; i < postfixLen; i++) {
      if (keys[keysLen - postfixLen + i].ch != postfix[i])
        return false;
    }

    // check everything inside is just a number
    for (i32 i = prefixLen; i < (keysLen - postfixLen); i++) {
      if (!IsNumeric(keys[i].ch))
        return false;
    }
    i32 c = 1;
    if (keysLen > (prefixLen + postfixLen)) {
      c = atoi(keys + prefixLen, keysLen - (prefixLen + postfixLen));
    }

    isMatch = true;
    *count = c;

    return true;
  }

  return false;
}

bool IsCtrlCommand(char ch) {
  if (keysLen == 1 && keys[0].ch == ch && keys[0].ctrl) {
    isMatch = true;
    return true;
  }
  return false;
}

struct Range {
  i32 left;
  i32 right;
  const char* motion;
  bool isSurrounder;
};

Range PerformMotion(const char* motion, i32 count) {
  Range range = {-1, -1, motion};
  if (strequal(motion, "gg")) {
    range.left = 0;
    range.right = Max(FindLineStart(pos) - 1, 0);
  }
  if (strequal(motion, "G")) {
    range.left = FindLineStart(pos);
    range.right = size - 1;
  }
  if (strequal(motion, "l")) {
    range.left = FindLineStart(pos);
    i32 right = pos;
    while (right < size && count > 0) {
      if (content[right] == '\n')
        count--;

      right++;
    }
    right--;
    range.right = right;
    return range;
  }
  if (strequal(motion, "w")) {
    range.left = pos;
    i32 end = pos;

    while (count > 0) {
      end = JumpWordForward(end);
      count--;
    }
    range.right = end;

    i32 last = range.right - 1;
    if (content[last] == '\n')
      last--;
    range.right = last;
    return range;
  }

  return range;
}
Range SurroundText(const char* motion, i32 count) {
  Range range = {-1, -1, motion, true};
  if (strequal(motion, "ib") || strequal(motion, "ab")) {
    range.left = pos;
    range.right = pos;
    char left = '(';
    char right = ')';
    while (content[range.left] != left && range.left > 0)
      range.left--;

    while (content[range.right] != right && range.right < (size - 1))
      range.right++;

    if (content[range.left] != left || content[range.right] != right) {
      range.left = -1;
      range.right = -1;
    } else if (strequal(motion, "ib")) {
      range.left++;
      range.right--;
    }
    return range;
  }
  if (strequal(motion, "iB") || strequal(motion, "aB")) {
    range.left = pos;
    range.right = pos;
    char left = '{';
    char right = '}';
    while (content[range.left] != left && range.left > 0)
      range.left--;

    while (content[range.right] != right && range.right < (size - 1))
      range.right++;

    if (content[range.left] != left || content[range.right] != right) {
      range.left = -1;
      range.right = -1;
    } else if (strequal(motion, "iB")) {
      range.left++;
      range.right--;
    }
    return range;
  }
  return range;
}

inline bool IsValid(Range range) {
  return range.left != -1 && range.right != -1;
}

void InsertNewLineWithOffset(i32 where, i32 offset);
void PerformOperatorOnRange(const char* op, Range range) {
  if (strequal(op, "d")) {
    BufferRemoveChars(range.left, range.right);

    if (strequal(range.motion, "l"))
      UpdateCursor(ApplyDesiredOffset(range.left));
    else if (range.left < pos)
      UpdateCursor(range.left);
  }

  if (strequal(op, "c")) {
    i32 offset = GetOffset(pos);
    BufferRemoveChars(range.left, range.right);

    if (!strequal(range.motion, "w"))
      InsertNewLineWithOffset(range.left - 1, offset);
    else
      UpdateCursor(range.left);

    EnterInsert();
  }
}

void HandleComplexCommands() {
  const char* motions[] = {"w", "l", "gg", "G"};
  const char* operators[] = {"d", "c"};
  const char* surrounders[] = {"ib", "ab", "iB", "aB"};

  i32 count;
  for (i32 j = 0; j < ArrayLength(operators); j++) {
    for (i32 i = 0; i < ArrayLength(motions); i++) {
      if (IsComplexCommand(operators[j], motions[i], &count)) {
        Range range = PerformMotion(motions[i], count);
        if (IsValid(range))
          PerformOperatorOnRange(operators[j], range);
      }
    }

    for (i32 i = 0; i < ArrayLength(surrounders); i++) {
      if (IsComplexCommand(operators[j], surrounders[i], &count)) {
        Range range = SurroundText(surrounders[i], count);
        if (IsValid(range))
          PerformOperatorOnRange(operators[j], range);
      }
    }
  }
}

bool CompareChar(char ch1, char ch2) {
  if (isCaseSensitiveSearch)
    return ch1 == ch2;
  else
    return ToLower(ch1) == ToLower(ch2);
}

void SearchNext(i32 from) {
  i32 termIndex = 0;
  for (i32 i = from; i < size - 1; i++) {
    if (CompareChar(content[i], searchTerm[termIndex])) {
      termIndex++;
    } else {
      termIndex = 0;
    }

    if (termIndex >= searchTermLen) {
      UpdateCursor(i - termIndex + 1);
      return;
    }
  }

  // stupid second search to find occurenced from start is non found from current
  for (i32 i = 0; i < size - 1; i++) {
    if (CompareChar(content[i], searchTerm[termIndex])) {
      termIndex++;
    } else {
      termIndex = 0;
    }

    if (termIndex >= searchTermLen) {
      UpdateCursor(i - termIndex + 1);
      return;
    }
  }
}

void HandleKeyPress() {
  isMatch = false;
  Key key = keys[keysLen - 1];
  if (key.ch == VK_ESCAPE) {
    mode = Normal;
    keysLen = 0;
  } else if (mode == Normal) {
    HandleComplexCommands();
    if (IsCommand("I")) {
      pos = FindLineStart(pos);
      EnterInsert();
    }
    if (IsCommand("i")) {
      EnterInsert();
    }
    if (IsCommand("n")) {
      SearchNext(pos + 1);
    }
    if (IsCommand("/")) {
      mode = SearchInput;
    }
    if (IsCommand("A")) {
      pos = FindLineEnd(pos);
      EnterInsert();
    }
    if (IsCommand("a")) {
      UpdateCursor(pos + 1);
      EnterInsert();
    }
    if (IsCommand("gg")) {
      UpdateCursorWithoutDesiredOffset(ApplyDesiredOffset(0));
    }
    if (IsCommand("G")) {
      UpdateCursorWithoutDesiredOffset(ApplyDesiredOffset(FindLineStart(size - 1)));
    }
    if (IsCommand("q")) {
      PostQuitMessage(0);
      isRunning = 0;
    }
    if (IsCommand("zz")) {
      OffsetCenteredOnCursor();
    }
    if (IsCommand("zf")) {
      isF = !isF;
      SetFullscreen(win, isF);
    }
    if (IsCommand("X")) {
      if (pos > 0) {
        BufferRemoveChars(pos - 1, pos - 1);
        UpdateCursor(pos - 1);
      }
    }
    if (IsCommand("x")) {
      if (pos < (size - 1))
        BufferRemoveChars(pos, pos);
    }
    if (IsCommand("C")) {
      i32 end = FindLineEnd(pos);
      BufferRemoveChars(pos, end - 1);
      EnterInsert();
    }
    if (IsCommand("D")) {
      i32 end = FindLineEnd(pos);
      BufferRemoveChars(pos, end - 1);
    }

    if (IsCtrlCommand('o'))
      AddLineInside();
    else if (IsCtrlCommand('s')) {
      WriteMyFile(path, content, size);
      isSaved = true;
    } else if (IsCommand("O"))
      AddLineAbove();
    else if (IsCommand("o"))
      AddLineBelow();

    if (IsCommand("l"))
      MoveRight();
    if (IsCommand("h"))
      MoveLeft();
    if (IsCommand("j"))
      MoveDown();
    if (IsCommand("k"))
      MoveUp();
    if (IsCommand("w"))
      UpdateCursor(JumpWordForward(pos));
    if (IsCommand("b"))
      UpdateCursor(JumpWordBackward(pos));
  }
  if (mode == Insert) {
    if (key.ch == VK_ESCAPE) {
      isMatch = 1;
      mode = Normal;
    }
    if (key.ch == VK_RETURN) {
      i32 offset = GetOffset(pos);

      char toInsert[255] = {'\n'};
      for (i32 i = 0; i < offset; i++)
        toInsert[i + 1] = ' ';
      BufferInsertChars(toInsert, offset + 1, pos);
      UpdateCursor(pos + offset + 1);
    }

    if (key.ch == VK_BACK) {
      if (pos > 0) {
        BufferRemoveChars(pos - 1, pos - 1);
        UpdateCursor(pos - 1);
      }
    }
  }
  if (mode == SearchInput) {

    isMatch = 1;
  }

  if (isMatch) {
    keysLen = 0;
  }
}

void AddKey(i64 ch) {
  keys[keysLen].alt = IsKeyPressed(VK_MENU);
  keys[keysLen].ctrl = IsKeyPressed(VK_CONTROL);
  keys[keysLen++].ch = ch;
  HandleKeyPress();
}

LRESULT OnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_MOUSEWHEEL:
    offset.target -= GET_WHEEL_DELTA_WPARAM(wParam);
    break;
  case WM_CHAR:
    if (ignoreNextCharEvent) {
      ignoreNextCharEvent = 0;
    } else if (mode == Insert) {
      if (wParam >= ' ' && wParam <= '}') {
        char chars[1] = {(char)wParam};
        BufferInsertChars(chars, 1, pos);
        UpdateCursor(pos + 1);
      }
    } else if (mode == SearchInput) {
      if (wParam == VK_RETURN) {
        mode = Normal;
      }
      if (wParam >= ' ' && wParam <= '}') {
        searchTerm[searchTermLen++] = (char)wParam;
        SearchNext(pos);
      }
      if (wParam == VK_BACK) {
        searchTermLen = Max(searchTermLen - 1, 0);
        if (searchTermLen > 0)
          SearchNext(pos);
      }
    } else if (mode == Normal) {
      AddKey(wParam);
    }
    break;
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    if (wParam == VK_SHIFT || wParam == VK_CONTROL)
      break;

    i64 ch = wParam;
    if (!IsKeyPressed(VK_SHIFT) && ch >= 'A' && ch <= 'Z')
      ch += 'a' - 'A';

    if ((mode == Normal && IsAlphaNumeric((char)ch)) || wParam == VK_ESCAPE) {
      AddKey(ch);
      ignoreNextCharEvent = true;
    }
  } break;
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
    break;
  }
  return DefWindowProc(handle, message, wParam, lParam);
}

void OpenWindow() {
  HINSTANCE instance = GetModuleHandle(0);
  WNDCLASSA windowClass = {0};
  windowClass.hInstance = instance;
  windowClass.lpfnWndProc = OnEvent;
  windowClass.lpszClassName = "MyClassName";
  windowClass.style = CS_VREDRAW | CS_HREDRAW;
  windowClass.hCursor = LoadCursor(0, IDC_ARROW);
  windowClass.hbrBackground = CreateSolidBrush(0);

  RegisterClassA(&windowClass);

  HDC screenDc = GetDC(0);

  int screenWidth = GetDeviceCaps(screenDc, HORZRES);
  int screenHeight = GetDeviceCaps(screenDc, VERTRES);

  int width = 1800;
  int height = 1000;
  const char* title = "Hell";

  win = CreateWindowA(windowClass.lpszClassName, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                      screenWidth - width, 0, width, height, 0, 0, instance, 0);

  ShowWindow(win, SW_SHOW);
  BOOL USE_DARK_MODE = TRUE;
  SUCCEEDED(DwmSetWindowAttribute(win, DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE,
                                  sizeof(USE_DARK_MODE)));
}

HFONT CreateFont(i32 fontSize, const char* name, i32 weight, i32 quiality) {
  int h = -MulDiv(fontSize, GetDeviceCaps(dc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
  return CreateFontA(h, 0, 0, 0,
                     // FW_BOLD,
                     weight,
                     0, // Italic
                     0, // Underline
                     0, // Strikeout
                     DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, quiality,
                     DEFAULT_PITCH, name);
}

// TODO: there is definitelly a better way, currently just trying to make calling part easy without
// variable arguments
char* strBuffer;
i32 strBufferCurrentPos;
void Append(const char* str) {
  char* buffer = strBuffer + strBufferCurrentPos;
  i32 l = 0;
  while (str[l] != '\0') {
    buffer[l] = str[l];
    l++;
  }
  strBufferCurrentPos += l;
}

void Append(char ch) {
  strBuffer[strBufferCurrentPos++] = ch;
}

void Append(int v) {
  char* buffer = strBuffer + strBufferCurrentPos;
  char temp[20] = {0};
  i32 l = 0;

  if (v == 0) {
    temp[l++] = '0';
  }

  while (v > 0) {
    temp[l++] = '0' + (v % 10);
    v /= 10;
  }

  for (i32 i = l - 1; i >= 0; i--)
    buffer[l - i - 1] = temp[i];

  strBufferCurrentPos += l;
}

void PrintFooter(f32 deltaMs) {

  if (mode == SearchInput)
    TextColors(0x00ffff, 0x000000);
  else
    TextColors(0x888888, 0x000000);

  if (searchTermLen > 0) {
    TextOut(dc, 20, canvas.height - 30, searchTerm, searchTermLen);
  } else if (mode == SearchInput)
    TextOut(dc, 20, canvas.height - 30, "SEARCH", strlen("SEARCH"));

  // Todo: this can be moved outside of function block, I can manipulate just lenth
  char buff[1024];
  strBuffer = buff;
  strBufferCurrentPos = 0;
  Append("Command: ");
  for (i32 i = 0; i < keysLen; i++) {
    if (keys[i].ch == VK_ESCAPE)
      Append("<ESC>");
    else if (keys[i].ch == VK_RETURN)
      Append("<ENTER>");
    else if (keys[i].ch == VK_BACK)
      Append("<BACK>");
    else
      Append((char)keys[i].ch);
  }

  Append(" ");

  if (isSaved)
    Append("Saved");
  else
    Append("Modified");

  Append(" ");

  Append("Frame: ");
  Append(i32(deltaMs));
  Append("ms ");

  SIZE s;
  const char* ch = "w";
  GetTextExtentPoint32A(dc, ch, 1, &s);

  TextColors(0x888888, 0x000000);
  TextOutA(dc, canvas.width - (strBufferCurrentPos * s.cx) - 20, canvas.height - 30, buff,
           strBufferCurrentPos);
}

f32 lerp(f32 from, f32 to, f32 v) {
  return (1 - v) * from + to * v;
}

extern "C" void WinMainCRTStartup() {
  PreventWindowsDPIScaling();
  dc = CreateCompatibleDC(0);
  OpenWindow();
  InitAnimations();

  HFONT f = CreateFont(15, "Consolas", FW_REGULAR, CLEARTYPE_NATURAL_QUALITY);
  SelectObject(dc, f);

  GetTextMetrics(dc, &m);
  // GLYPHSET* set;
  // set = (GLYPHSET*)valloc(GetFontUnicodeRanges(dc, nullptr));
  // GetFontUnicodeRanges(dc, set);
  // i32 r = set->cGlyphsSupported;

  HDC windowDC = GetDC(win);

  i64 fileSize = GetMyFileSize(path);
  char* temp = (char*)valloc(fileSize);
  ReadFileInto(path, fileSize, temp);
  capacity = fileSize * 2;
  content = (char*)valloc(capacity);
  size = 0;
  for (i32 i = 0; i < fileSize; i++) {
    if (temp[i] != '\r')
      content[size++] = temp[i];
  }
  vfree(temp);

  i64 frameStart = GetPerfCounter();
  f32 freq = (f32)GetPerfFrequency();
  while (isRunning) {
    MSG msg;

    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    memset(canvas.pixels, 0x00, canvas.width * canvas.height * 4);

    f32 pageHeight = GetLinesCount() * m.tmHeight * lineHeight + padding * 2;
    if (pageHeight > canvas.height) {
      i32 scrollHeight = f32(canvas.height * canvas.height) / f32(pageHeight);

      f32 maxOffset = pageHeight - canvas.height;
      f32 maxScrollY = canvas.height - scrollHeight;
      f32 scrollY = lerp(0, maxScrollY, offset.current / maxOffset);

      i32 scrollWidth = 10;
      PaintRect(canvas.width - scrollWidth, scrollY, scrollWidth, scrollHeight, 0x555555);
    }

    TextColors(0xd0d0d0, 0x000000);

    i32 x = padding;
    f32 y = padding - offset.current;

    SIZE s;
    const char* ch = "w";
    GetTextExtentPoint32A(dc, ch, 1, &s);

    i32 i = 0;
    while (i < size) {
      i32 lineStart = i;
      while (content[i] != '\n' && i < size)
        i++;

      TextOutA(dc, x, (i32)y, content + lineStart, i - lineStart);

      if (i >= size)
        break;
      i++;
      y += (f32)m.tmHeight * lineHeight;
    }

    CursorPos p = GetCursorPos();
    u32 cursorbg = 0xFFDC32;
    if (mode == Insert)
      cursorbg = 0xFF3269;
    TextColors(0x000000, cursorbg);
    i32 cursorX = x + p.col * s.cx;
    i32 cursorY = padding + p.row * m.tmHeight * lineHeight - offset.current;
    if (mode == Insert)
      PaintRect(cursorX - 2, cursorY, 2, m.tmHeight, cursorbg);
    else {
      PaintRect(cursorX, cursorY, s.cx, m.tmHeight, cursorbg);

      TextOutA(dc, cursorX, cursorY, content + pos, 1);
    }
    i64 frameEnd = GetPerfCounter();

    f32 deltaSec = (frameEnd - frameStart) / freq;
    PrintFooter(deltaSec * 1000.0f);

    UpdateSpring(&offset, deltaSec);

    StretchDIBits(windowDC, 0, 0, canvas.width, canvas.height, 0, 0, canvas.width, canvas.height,
                  canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
    frameStart = frameEnd;
  }

  ExitProcess(0);
}
