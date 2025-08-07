#define WIN32_LEAN_AND_MEAN
#include <dwmapi.h>
#include <stdint.h>
#include <windows.h>

#define ArrayLength(array) (sizeof(array) / sizeof(array[0]))
#define KB(v) (1024 * v)

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
bool isInitialized;

HMODULE libModule;
typedef int64_t i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint8_t u8;
typedef int8_t i8;
typedef float f32;
typedef wchar_t wc;

enum Mode { Normal, Insert, Visual, VisualLine, SearchInput, OutlineInput };
enum SearchDirection { down, up };
char searchTerm[255];
i32 searchTermLen;
SearchDirection searchDirection;

char* buildLogs;
i32 buildLogsLen;
bool isFailedToBuild = false;
i64 buildTimeMs = 0;
i64 formatTimeMs = 0;
// TODO: there is definitelly a better way, currently just trying to make calling part easy without
// variable arguments
char strBuffer[KB(2)];
i32 strBufferCurrentPos;

char pattern[200] = "";
i32 patternLen = 0;
i32 selectedOutlineItem = 0;

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

typedef struct MyBitmap {
  i32 width;
  i32 height;
  i32 bytesPerPixel;
  u32* pixels;
} MyBitmap;

enum ChangeType { Added, Removed }; // Replaced

struct Change {
  ChangeType type;
  int at;

  // shows how much text added in a change, comes first in text, can be zero
  int addedTextSize;

  // shows how much text removed in a change, comes second in text, can be zero
  int removedTextSize;

  // two fields are non-zero for Replaced ChangeType
  char text[];
};

struct ChangeArena {
  Change* contents;
  int lastChangeIndex; // -1 is the default value
  int capacity;
  int changesCount;
};

BITMAPINFO bitmapInfo;
MyBitmap canvas;
HBITMAP canvasBitmap;

f32 padding = 20.0f;
f32 lineHeight = 1.1f;
i32 pos = 0;
i32 selectionStart = -1;
i32 desiredOffset = 0;
HDC dc;
HDC windowDC;
HWND win;
Mode mode = Normal;
char* content;
i64 size;
i64 capacity;
ChangeArena changes;
Spring offset;
bool isSaved = true;
TEXTMETRICA m;

const char* files[] = {
    "play.c",
    "main.cpp",
    "progress.txt",
    "build.bat",
};
const char* currentPath;

typedef struct Rect {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
} Rect;

typedef void Render(HDC dc, MyBitmap* bitmap, Rect rect, float d);
typedef void Teardown();
Render* render;
Teardown* teardown;

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
u32 IsWhitespace(char ch) {
  return ch == ' ' || ch == '\n';
}

u32 IsNumeric(char ch) {
  return (ch >= '0' && ch <= '9');
}

u32 IsAlphaNumeric(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

u32 IsAsciiChar(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool IsPrintable(char ch) {
  return ch >= ' ' && ch <= '}';
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

int IndexOf(char* sub, char* overall) {
  i32 termIndex = 0;
  i32 searchTermLen = strlen(sub);
  for (i32 i = 0; overall[i]; i++) {
    if (ToLower(overall[i]) == ToLower(sub[termIndex])) {
      termIndex++;
    } else {
      termIndex = 0;
    }

    if (termIndex >= searchTermLen) {
      return i - searchTermLen + 1;
    }
  }
  return -1;
}

int IndexOfCaseSensitive(char* sub, char* overall) {
  i32 termIndex = 0;
  i32 searchTermLen = strlen(sub);
  for (i32 i = 0; overall[i]; i++) {
    if (overall[i] == sub[termIndex]) {
      termIndex++;
    } else {
      termIndex = 0;
    }

    if (termIndex >= searchTermLen) {
      return i - searchTermLen + 1;
    }
  }
  return -1;
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
void memmove(void* dest, void* src, size_t n) {
  char* csrc = (char*)src;
  char* cdest = (char*)dest;

  // TODO: WTF is valloc doing here? Use a pre-allocated buffer for this
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
  changes.lastChangeIndex = -1;
  changes.changesCount = 0;
}

void WriteMyFile(const char* path, char* content, int size) {
  HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

  DWORD bytesWritten;
  int res = WriteFile(file, content, size, &bytesWritten, 0);
  CloseHandle(file);
}

char* ReadFromClipboard(HWND window, i32* size) {
  OpenClipboard(window);
  HANDLE hClipboardData = GetClipboardData(CF_TEXT);
  char* pchData = (char*)GlobalLock(hClipboardData);
  char* res;
  if (pchData) {
    i32 len = strlen(pchData);
    res = (char*)valloc(len);
    memmove(res, pchData, len);
    GlobalUnlock(hClipboardData);
    *size = len;
  } else {
    OutputDebugStringA("Failed to capture clipboard\n");
  }
  CloseClipboard();
  return res;
}

// https://www.codeproject.com/Articles/2242/Using-the-Clipboard-Part-I-Transferring-Simple-Tex
void WriteToClipboard(HWND window, char* text, i32 len) {
  if (OpenClipboard(window)) {
    EmptyClipboard();

    HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, len + 1);

    char* pchData = (char*)GlobalLock(hClipboardData);

    memmove(pchData, text, len);
    pchData[len] = '\0';

    GlobalUnlock(hClipboardData);

    SetClipboardData(CF_TEXT, hClipboardData);

    CloseClipboard();
  }
}

void RunCommand(char* cmd, char* output, int* len) {
  HANDLE hRead, hWrite;
  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

  if (!CreatePipe(&hRead, &hWrite, &sa, KB(128)))
    return;

  // Ensure the read handle to the pipe is not inherited.
  SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

  PROCESS_INFORMATION pi;
  STARTUPINFO si = {sizeof(STARTUPINFO)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = hWrite;
  si.hStdError = hWrite;
  si.hStdInput = NULL;

  if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
    CloseHandle(hWrite);
    CloseHandle(hRead);
    return;
  }

  CloseHandle(hWrite);

  WaitForSingleObject(pi.hProcess, INFINITE);

  if (output && len) {
    DWORD bytesRead = 0, totalRead = 0;
    while (ReadFile(hRead, output + totalRead, 1, &bytesRead, NULL)) {
      totalRead += bytesRead;
    }
    output[totalRead] = '\0';
    *len = totalRead;
  }

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(hRead);
}

typedef struct CtagEntry {
  char name[100];
  char filename[50];
  char pattern[100];
  char type;
  char def[200];
} CtagEntry;

CtagEntry* entries;
int entriesCount;

void ReadCtagsFile() {
  if (!entries)
    entries = (CtagEntry*)valloc(2000 * sizeof(CtagEntry));
  else
    memset(entries, 0, 2000 * sizeof(CtagEntry));

  entriesCount = 0;
  int fileSize = GetMyFileSize("tags");
  char* memory = (char*)valloc(fileSize);
  ReadFileInto("tags", fileSize, memory);

  int pos = 0;
  while (memory[pos] == '!') {
    while (memory[pos] != '\n')
      pos++;
    pos++;
  }
  // name  filename  pattern  type  info\n
  int currentField = 0;
  int fieldStart = pos;
  while (pos < fileSize) {
    while (memory[pos] != '\n') {

      if (memory[pos] == '\t') {
        char* destination;
        if (currentField == 0)
          destination = (char*)&entries[entriesCount].name;
        if (currentField == 1)
          destination = (char*)&entries[entriesCount].filename;
        if (currentField == 2)
          destination = (char*)&entries[entriesCount].pattern;
        if (currentField == 3)
          destination = (char*)&entries[entriesCount].type;

        if (currentField == 2) {
          char* start = &memory[fieldStart] + 2;
          int len = pos - fieldStart - 2 - 3;
          if (memory[pos - 4] == '$')
            len -= 1;
          memmove(destination, start, len);

        } else
          memmove(destination, &memory[fieldStart], pos - fieldStart);

        currentField++;
        fieldStart = pos + 1;
      }
      pos++;
    }

    memmove(&entries[entriesCount].def, &memory[fieldStart], pos - fieldStart);

    currentField = 0;
    entriesCount++;
    pos++;
    fieldStart = pos;
  }

  vfree(memory);
}

//
// Changes
//

i32 ChangeSize(Change* c) {
  return sizeof(Change) + c->addedTextSize + c->removedTextSize;
}

Change* NextChange(Change* c) {
  return (Change*)(((u8*)c) + ChangeSize(c));
}

Change* GetChangeAt(ChangeArena* changes, int at) {
  Change* c = changes->contents;

  for (int i = 0; i < at; i++)
    c = NextChange(c);

  return c;
}

void DoubleCapacityIfFull() {
  char* currentStr = content;
  capacity = (capacity == 0) ? 4 : (capacity * 2);
  content = (char*)valloc(capacity);
  if (currentStr) {
    memmove(content, currentStr, size);
    vfree(currentStr);
  }
}

void DoubleChangeArenaCapacity(ChangeArena* changes) {
  i32 capacity = changes->capacity * 2;
  Change* newContents = (Change*)valloc(capacity);
  memmove(newContents, changes->contents, changes->capacity);
  vfree(changes->contents);
  changes->contents = newContents;
  changes->capacity = capacity;
}

int GetChangeArenaSize(ChangeArena* arena) {
  Change* c = arena->contents;
  int res = 0;
  for (int i = 0; i < arena->changesCount; i++) {
    res += ChangeSize(c);
    c = NextChange(c);
  }
  return res;
}

Change* AddNewChange(int changeSize) {
  int size = GetChangeArenaSize(&changes) + changeSize;
  if (size >= changes.capacity)
    DoubleChangeArenaCapacity(&changes);

  changes.lastChangeIndex += 1;
  changes.changesCount = changes.lastChangeIndex + 1;

  return GetChangeAt(&changes, changes.lastChangeIndex);
}

void BufferRemoveChars(int from, int to) {
  int num_to_shift = size - (to + 1);

  memmove(content + from, content + to + 1, num_to_shift);

  size -= (to - from + 1);
  isSaved = false;
}

void BufferInsertChars(char* chars, i32 len, i32 at) {
  while (size + len >= capacity)
    DoubleCapacityIfFull();

  char* file = content;
  if (size != 0) {

    size += len;

    char* from = file + at;
    char* to = file + at + len;
    memmove(to, from, size - at);
  } else {
    size = len;
  }

  for (i32 i = at; i < at + len; i++) {
    file[i] = chars[i - at];
  }
  isSaved = false;
}

void ApplyChange(Change* c) {
  if (c->type == Added)
    BufferInsertChars(c->text, c->addedTextSize, c->at);
  if (c->type == Removed)
    BufferRemoveChars(c->at, c->at + c->addedTextSize - 1);
  isSaved = false;
}

void UndoChange(Change* c) {
  if (c->type == Added)
    BufferRemoveChars(c->at, c->at + c->addedTextSize - 1);
  if (c->type == Removed)
    BufferInsertChars(c->text, c->addedTextSize, c->at);
  isSaved = false;
}

//
// Change API methods below
//
Change* UndoLastChange() {
  int lastChangeAt = changes.lastChangeIndex;
  if (lastChangeAt >= 0) {
    Change* changeToUndo = GetChangeAt(&changes, lastChangeAt);
    changes.lastChangeIndex--;

    UndoChange(changeToUndo);
    return changeToUndo;
  }
  return 0;
}

Change* RedoLastChange() {
  int pendingChangeAt = changes.lastChangeIndex + 1;
  if (pendingChangeAt < changes.changesCount) {
    Change* c = GetChangeAt(&changes, pendingChangeAt);

    ApplyChange(c);
    changes.lastChangeIndex++;
    return c;
  }
  return 0;
}

void RemoveChars(int from, int to) {
  int changeSize = sizeof(Change) + (to - from + 1);
  Change* c = AddNewChange(changeSize);

  c->type = Removed;
  c->at = from;
  c->addedTextSize = to - from + 1;
  c->removedTextSize = 0;
  memmove(c->text, content + from, c->addedTextSize);

  ApplyChange(c);
}

void InsertChars(char* chars, i32 len, i32 at) {
  int changeSize = sizeof(Change) + len;
  Change* c = AddNewChange(changeSize);

  c->type = Added;
  c->at = at;
  c->addedTextSize = len;
  c->removedTextSize = 0;
  memmove(c->text, chars, c->addedTextSize);

  ApplyChange(c);
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

f32 Abs(f32 v) {
  if (v < 0)
    return -v;
  return v;
}

i32 pow(i32 b, i32 p) {
  i32 res = 1;
  for (i32 i = 0; i < p; i++)
    res *= b;
  return res;
}

i32 atoi(char* buff) {
  i32 res = 0;
  i32 i = 0;
  char numbers[20] = {0};

  while (buff[i] != '\0' && buff[i] >= '0' && buff[i] <= '9') {
    numbers[i] = buff[i];
    i++;
  }
  i32 len = i;
  i--;
  while (i >= 0) {
    res += (numbers[i] - '0') * pow(10, len - i - 1);
    i--;
  }

  return res;
}

struct CursorPos {
  i32 row;
  i32 col;
};

void ReadFileIntoBuffer(const char* filepath) {
  currentPath = filepath;
  i64 fileSize = GetMyFileSize(currentPath);
  char* temp = (char*)valloc(fileSize);
  ReadFileInto(currentPath, fileSize, temp);
  capacity = fileSize * 2;
  content = (char*)valloc(capacity);
  size = 0;
  for (i32 i = 0; i < fileSize; i++) {
    if (temp[i] != '\r')
      content[size++] = temp[i];
  }
  vfree(temp);

  pos = 0;
  offset.current = offset.target = 0;
}

CursorPos GetCursorPos(i32 p) {
  CursorPos res = {0};
  i32 rowStartAt = 0;
  for (i32 i = 0; i < p; i++) {
    if (content[i] == '\n') {
      res.row++;
      rowStartAt = i + 1;
    }
  }
  res.col = p - rowStartAt;
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
  CursorPos p = GetCursorPos(pos);
  f32 cursorY = p.row * lineHeightPx + padding;
  offset.target = Max(cursorY - (canvas.height / 2.0f), 0);
}
void ScrollIntoCursor() {
  i32 itemsToLookAhead = 5;
  f32 lineHeightPx = m.tmHeight * lineHeight;

  CursorPos p = GetCursorPos(pos);

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
  p = Max(p - 1, 0);
  i32 isStartedAtWhitespace = IsWhitespace(text[p]);

  while (p > 0 && IsWhitespace(text[p]))
    p--;

  if (IsAlphaNumeric(text[p])) {
    while (p > 0 && IsAlphaNumeric(text[p]))
      p--;
  } else {
    while (p > 0 && IsPunctuation(text[p]))
      p--;
  }
  if (p != 0)
    p++;

  if (!isStartedAtWhitespace) {
    while (p > 0 && IsWhitespace(text[p]))
      p--;
  }

  return p;
}

i32 GetOffset(i32 p) {
  i32 start = FindLineStart(p);
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

void FormatCode() {
  i64 formatStart = GetPerfCounter();
  WriteMyFile(currentPath, content, size);
  // SaveBuffer(selectedBuffer);
  strBufferCurrentPos = 0;

  Append("cmd /c clang-format ");

  Append(currentPath);
  Append(" --cursor=");
  Append(pos);
  Append('\0');

  char* output = (char*)valloc(KB(100));
  i32 outputLen = 0;
  RunCommand(strBuffer, output, &outputLen);

  if (outputLen > 0) {
    char* t = output;
    i32 start = 0;
    while (t[start] != '\n')
      start++;
    start++;
    i32 newLen = outputLen - start;
    // Replace text action
    memmove(content, output + start, newLen);
    size = newLen;
    isSaved = false;

    // 12 here is the index of new cursor position in JSON response { "Cursor": 36
    // I don't want to parse entire JSON yet
    int newCursor = atoi(output + 12);
    UpdateCursor(newCursor);
    vfree(output);
    formatTimeMs = round(f32(GetPerfCounter() - formatStart) * 1000.0f / (f32)GetPerfFrequency());
  }
}
void FreeLib() {
  if (libModule) {
    if (teardown) {
      teardown();
    }
    render = nullptr;
    teardown = nullptr;
    FreeLibrary(libModule);
  }
}

void LoadLib() {
  const char* expectedlogs = "   Creating library build\\play.lib and object build\\play.exp\r\n";

  if (strequal(buildLogs, expectedlogs)) {
    isFailedToBuild = false;
    libModule = LoadLibrary("build\\play.dll");
    if (libModule) {
      render = (Render*)GetProcAddress(libModule, "GetSome");
      teardown = (Teardown*)GetProcAddress(libModule, "Teardown");
    }
  } else {
    isFailedToBuild = true;
  }
}
void RunCode(const char* path) {
  i64 buildStart = GetPerfCounter();
  WriteMyFile(currentPath, content, size);
  isSaved = true;
  strBufferCurrentPos = 0;
  Append("cmd /c ");
  Append(path);
  Append('\0');
  RunCommand(strBuffer, buildLogs, &buildLogsLen);

  buildTimeMs = round(f32(GetPerfCounter() - buildStart) * 1000.0f / (f32)GetPerfFrequency());
  RunCommand((char*)"build\\main.exe", 0, 0);
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
  InsertChars(toInsert, offset + 1, where);
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
      char ks[1024];
      i32 l = keysLen - (prefixLen + postfixLen);
      Key* start = keys + prefixLen;
      for (i32 i = 0; i < l; i++)
        ks[i] = start[i].ch;
      c = atoi(ks);
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

bool IsAltCommand(char ch) {
  if (keysLen == 1 && keys[0].ch == ch && keys[0].alt) {
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

Range GetSelectionRange() {
  i32 selectionLeft = Min(selectionStart, pos);
  i32 selectionRight = Max(selectionStart, pos);

  if (mode == Visual) {
    return Range{.left = selectionLeft, .right = selectionRight};
  } else if (mode == VisualLine) {
    selectionLeft = FindLineStart(selectionLeft);
    selectionRight = FindLineEnd(selectionRight);

    return Range{.left = selectionLeft, .right = selectionRight};
  }
  return {};
}

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
    i32 pot = last;

    while (content[pot] == ' ')
      pot--;

    if (content[pot] == '\n') {
      range.right = pot - 1;
    } else
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
  if (strequal(motion, "iw") || strequal(motion, "aw")) {
    range.left = pos;
    range.right = pos;

    while (range.left > 0 && IsAlphaNumeric(content[range.left - 1]))
      range.left--;
    while (range.right < (size - 1) && IsAlphaNumeric(content[range.right + 1]))
      range.right++;

    if (strequal(motion, "aw")) {
      if (range.right < (size - 1) && content[range.right + 1] == ' ') {
        range.right++;

        while (range.right < (size - 1) && content[range.right + 1] == ' ')
          range.right++;
      }
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
    RemoveChars(range.left, range.right);

    if (strequal(range.motion, "l"))
      UpdateCursor(ApplyDesiredOffset(range.left));
    else if (range.left < pos)
      UpdateCursor(range.left);
  }

  if (strequal(op, "c")) {
    i32 offset = GetOffset(pos);
    RemoveChars(range.left, range.right);

    if (!strequal(range.motion, "w") && !range.isSurrounder)
      InsertNewLineWithOffset(range.left - 1, offset);
    else
      UpdateCursor(range.left);

    EnterInsert();
  }
}

void HandleComplexCommands() {
  const char* motions[] = {"w", "l", "gg", "G"};
  const char* operators[] = {"d", "c"};
  const char* surrounders[] = {"ib", "ab", "iB", "aB", "iw", "aw"};

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

void UpdateCursorOnTreeMotion(i32 newPos) {
  i32 p = Max(newPos, FindLineStart(newPos) + GetOffset(newPos));
  UpdateCursorWithoutDesiredOffset(ApplyDesiredOffset(p));
}

void MoveDownTreewise() {
  i32 currentOffset = GetOffset(pos);
  i32 p = FindLineEnd(pos) + 1;
  while (p < (size - 1)) {
    i32 offset = GetOffset(p);
    if (offset <= currentOffset && content[p] != '\n')
      break;

    p = FindLineEnd(p) + 1;
  }
  UpdateCursorOnTreeMotion(p);
}

void MoveUpTreewise() {
  i32 currentOffset = GetOffset(pos);
  i32 p = FindLineStart(FindLineStart(pos) - 1);
  while (p > 0) {
    i32 offset = GetOffset(p);
    if (offset <= currentOffset && content[p] != '\n')
      break;

    p = FindLineStart(p - 1);
  }
  if (p >= 0)
    UpdateCursorOnTreeMotion(p);
}

void MoveLeftTreewise() {
  i32 currentOffset = GetOffset(pos);
  if (currentOffset == 0)
    return;
  i32 p = FindLineStart(FindLineStart(pos) - 1);
  while (p > 0) {
    i32 offset = GetOffset(p);
    if (offset < currentOffset && content[p] != '\n')
      break;

    p = FindLineStart(p - 1);
  }
  if (p >= 0)
    UpdateCursorOnTreeMotion(p);
}

void MoveRightTreewise() {
  i32 currentOffset = GetOffset(pos);
  i32 p = FindLineEnd(pos) + 1;
  if (GetOffset(p) > currentOffset)
    UpdateCursorOnTreeMotion(p);
}

void HandleNormalAndVisualMotions() {
  if (IsCtrlCommand('h'))
    MoveLeftTreewise();
  else if (IsCommand("h"))
    MoveLeft();
  else if (IsCtrlCommand('l'))
    MoveRightTreewise();
  else if (IsCommand("l"))
    MoveRight();
  else if (IsCtrlCommand('j'))
    MoveDownTreewise();
  else if (IsCommand("j"))
    MoveDown();
  else if (IsCtrlCommand('k'))
    MoveUpTreewise();
  else if (IsCommand("k"))
    MoveUp();

  if (IsCommand("w"))
    UpdateCursor(JumpWordForward(pos));
  if (IsCommand("b"))
    UpdateCursor(JumpWordBackward(pos));
  if (IsCommand("gg")) {
    UpdateCursorWithoutDesiredOffset(ApplyDesiredOffset(0));
  }
  if (IsCommand("G")) {
    UpdateCursorWithoutDesiredOffset(ApplyDesiredOffset(FindLineStart(size - 1)));
  }
  if (IsCommand("0")) {
    UpdateCursor(FindLineStart(pos));
  }
  if (IsCommand("$")) {
    UpdateCursor(FindLineEnd(pos));
  }
  if (IsCommand("^")) {
    UpdateCursor(FindLineStart(pos) + GetOffset(pos));
  }
}

Range RemoveSelection() {
  Range range = GetSelectionRange();

  RemoveChars(range.left, range.right);
  UpdateCursor(range.left);
  return range;
}

enum PasteMethod { DoNotPasteOnNewLine, PasteOnNewLine };

void PasteIntoCurrentPosition(PasteMethod method) {
  i32 size;
  char* clipData = ReadFromClipboard(win, &size);
  bool isPastingOnNewLine = false;

  if (method == PasteOnNewLine) {
    for (i32 i = 0; i < size; i++) {
      if (clipData[i] == '\n') {
        isPastingOnNewLine = true;
        break;
      }
    }
  }
  i32 target = pos;
  if (isPastingOnNewLine)
    target = FindLineEnd(pos) + 1;

  InsertChars(clipData, size, target);
  UpdateCursor(target + size);
  vfree(clipData);
}

void HandleKeyPress() {
  isMatch = false;
  Key key = keys[keysLen - 1];
  if (key.ch == VK_ESCAPE) {
    mode = Normal;
    keysLen = 0;
  } else if (mode == Normal) {
    HandleComplexCommands();
    HandleNormalAndVisualMotions();

    if (keysLen == 1 && key.alt) {
      i32 index = key.ch - '1';
      if (index < ArrayLength(files)) {
        WriteMyFile(currentPath, content, size);
        ReadFileIntoBuffer(files[index]);
        isSaved = true;
        isMatch = true;
      }
    }
    if (IsCommand(" s")) {
      WriteMyFile(currentPath, content, size);

      RunCommand((char*)"cmd /c ctags *", 0, 0);
      ReadCtagsFile();
      mode = OutlineInput;
    }
    if (IsCommand("v")) {
      mode = Visual;
      selectionStart = pos;
      ignoreNextCharEvent = true;
    }
    if (IsCommand("u"))
      UndoLastChange();
    if (IsCommand("U"))
      RedoLastChange();

    if (IsAltCommand('r')) {
      RunCode("build.bat");
    }
    if (IsAltCommand('t')) {
      FreeLib();
      Sleep(12);
      RunCode("lib.bat");
      LoadLib();
    }
    if (IsAltCommand('f')) {
      FormatCode();
    }
    if (IsCommand("p")) {
      PasteIntoCurrentPosition(PasteOnNewLine);
    }

    if (IsCommand("V")) {
      mode = VisualLine;
      selectionStart = pos;
      ignoreNextCharEvent = true;
    }

    if (IsCommand("I")) {
      UpdateCursor(FindLineStart(pos));
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
        RemoveChars(pos - 1, pos - 1);
        UpdateCursor(pos - 1);
      }
    }
    if (IsCommand("x")) {
      if (pos < (size - 1))
        RemoveChars(pos, pos);
    }
    if (IsCommand("C")) {
      i32 end = FindLineEnd(pos);
      RemoveChars(pos, end - 1);
      EnterInsert();
    }
    if (IsCommand("D")) {
      i32 end = FindLineEnd(pos);
      RemoveChars(pos, end - 1);
    }

    if (IsCtrlCommand('o'))
      AddLineInside();
    else if (IsCtrlCommand('s')) {
      WriteMyFile(currentPath, content, size);
      isSaved = true;
    } else if (IsCommand("O"))
      AddLineAbove();
    else if (IsCommand("o"))
      AddLineBelow();

  } else if (mode == Visual) {
    HandleNormalAndVisualMotions();
    if (IsCommand("d")) {
      RemoveSelection();
      mode = Normal;
    }
    if (IsCommand("c")) {
      RemoveSelection();
      EnterInsert();
    }
    if (IsCommand("o")) {
      i32 tmp = selectionStart;
      selectionStart = pos;
      pos = tmp;
    }
    if (IsCommand("y")) {
      Range range = GetSelectionRange();
      WriteToClipboard(win, content + range.left, range.right - range.left + 1);
      mode = Normal;
    }
    if (IsCommand("p")) {
      RemoveSelection();
      PasteIntoCurrentPosition(PasteOnNewLine);
      mode = Normal;
    }
  } else if (mode == VisualLine) {
    HandleNormalAndVisualMotions();
    if (IsCommand("d")) {
      RemoveSelection();
      mode = Normal;
    }
    if (IsCommand("c")) {
      i32 offset = GetOffset(pos);
      Range range = RemoveSelection();
      InsertNewLineWithOffset(range.left - 1, offset);
      EnterInsert();
    }
    if (IsCommand("o")) {
      i32 tmp = selectionStart;
      selectionStart = pos;
      pos = tmp;
    }
    if (IsCommand("y")) {
      Range range = GetSelectionRange();
      WriteToClipboard(win, content + range.left, range.right - range.left + 1);
      mode = Normal;
    }
    if (IsCommand("p")) {
      RemoveSelection();
      PasteIntoCurrentPosition(PasteOnNewLine);
      mode = Normal;
    }
  } else if (mode == Insert) {
    if (key.ch == VK_ESCAPE) {
      isMatch = 1;
      mode = Normal;
    }
    if (key.ch == VK_RETURN) {
      i32 offset = GetOffset(pos);

      char toInsert[255] = {'\n'};
      for (i32 i = 0; i < offset; i++)
        toInsert[i + 1] = ' ';
      InsertChars(toInsert, offset + 1, pos);
      UpdateCursor(pos + offset + 1);
    }

    if (key.ch == VK_BACK) {
      if (pos > 0) {
        RemoveChars(pos - 1, pos - 1);
        UpdateCursor(pos - 1);
      }
    }
    if (key.ch == 'w' && key.ctrl) {
      i32 to = JumpWordBackward(pos);
      RemoveChars(to, pos - 1);
      UpdateCursor(to);
    } else if (key.ch == 'v' && key.ctrl) {
      PasteIntoCurrentPosition(DoNotPasteOnNewLine);

    } else if (key.ch >= ' ' && key.ch <= '}') {
      char chars[1] = {(char)key.ch};
      InsertChars(chars, 1, pos);
      UpdateCursor(pos + 1);
    }
  } else if (mode == SearchInput) {
    if (key.ch == VK_BACK) {
      searchTermLen = Max(searchTermLen - 1, 0);
      if (searchTermLen > 0)
        SearchNext(pos);
    }
    if (key.ch == VK_RETURN) {
      mode = Normal;
    }

    if (key.ch == 'w' && key.ctrl) {
      searchTermLen = 0;
    } else if (IsPrintable(key.ch)) {
      searchTerm[searchTermLen++] = (char)key.ch;
      SearchNext(pos);
    }
    isMatch = 1;
  } else if (mode == OutlineInput) {

    if (key.ch == VK_BACK) {
      patternLen = Max(patternLen - 1, 0);
      pattern[patternLen] = '\0';
    }
    if (key.ch == VK_RETURN) {

      i32 currentItem = 0;
      for (i32 i = 0; i < entriesCount; i++) {
        char* f = entries[i].name;
        i32 index = IndexOf(pattern, entries[i].name);
        if (index >= 0) {
          if (currentItem == selectedOutlineItem) {

            if (!strequal(entries[i].filename, currentPath)) {
              WriteMyFile(currentPath, content, size);
              char* path;
              for (i32 f = 0; f < ArrayLength(files); f++) {
                if (strequal(files[f], entries[i].filename)) {
                  ReadFileIntoBuffer(files[f]);
                  isSaved = true;
                  break;
                }
              }
            }

            i32 positionInFile = IndexOfCaseSensitive(entries[i].pattern, content);
            UpdateCursor(positionInFile);

            break;
          } else {
            currentItem++;
          }
        }
      }
      mode = Normal;
    }

    if (key.ch == 'j' && key.ctrl) {
      selectedOutlineItem++;
    } else if (key.ch == 'k' && key.ctrl) {
      selectedOutlineItem = Max(0, selectedOutlineItem - 1);
    } else if (key.ch == 'w' && key.ctrl) {
      patternLen = 0;
      pattern[patternLen] = '\0';
    } else if (IsPrintable(key.ch)) {
      pattern[patternLen++] = (char)key.ch;
      pattern[patternLen] = '\0';
    }
    isMatch = 1;
  }

  if (isMatch) {
    keysLen = 0;
  }
}

void Draw(f32 deltaSec);
void AddKey(i64 ch) {
  keys[keysLen].alt = IsKeyPressed(VK_MENU);
  keys[keysLen].ctrl = IsKeyPressed(VK_CONTROL);
  keys[keysLen++].ch = ch;
  HandleKeyPress();
  InvalidateRect(win, 0, false);
}

LRESULT OnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_MOUSEWHEEL:
    offset.target -= GET_WHEEL_DELTA_WPARAM(wParam);
    break;
  case WM_CHAR:
    if (ignoreNextCharEvent)
      ignoreNextCharEvent = 0;
    else
      AddKey(wParam);

    break;
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    if (wParam == VK_SHIFT || wParam == VK_CONTROL ||
        (IsKeyPressed(VK_SHIFT) && !IsAsciiChar(wParam)))
      break;

    i64 ch = wParam;
    if (!IsKeyPressed(VK_SHIFT))
      ch = ToLower(ch);

    // todo: ugly way to detect shortcuts, because <C-w> is not char WM_CHAR
    if (((mode == Normal || mode == Visual || mode == VisualLine) && IsAlphaNumeric((char)ch)) ||
        (mode == SearchInput && ch == 'w' && IsKeyPressed(VK_CONTROL)) ||
        (mode == OutlineInput && IsKeyPressed(VK_CONTROL)) ||
        (mode == Insert && ch == 'w' && IsKeyPressed(VK_CONTROL)) ||
        (mode == Insert && ch == 'v' && IsKeyPressed(VK_CONTROL))) {
      AddKey(ch);
      ignoreNextCharEvent = true;
    }
  } break;
  case WM_SYSCHAR:
    // disabled the annoying sound of unhandled alt+key
    return 0;
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    isRunning = 0;
    break;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(handle, &ps);
    Draw(0.33333);
    EndPaint(handle, &ps);

  } break;
  case WM_SIZE:
    Size(lParam);
    if (isInitialized)
      Draw(0);
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

void PrintFooter(f32 deltaMs) {

  if (mode == SearchInput)
    TextColors(0x00ffff, 0x000000);
  else
    TextColors(0x888888, 0x000000);

  if (searchTermLen > 0) {
    TextOut(dc, 20, canvas.height - 30, searchTerm, searchTermLen);
  } else if (mode == SearchInput)
    TextOut(dc, 20, canvas.height - 30, "SEARCH", strlen("SEARCH"));

  strBufferCurrentPos = 0;
  Append("Frame: ");
  Append(i32(deltaMs));
  Append("ms ");

  if (buildTimeMs != 0) {
    if (isFailedToBuild)
      Append("Build fail: ");
    else
      Append("Build ok: ");

    Append((i32)buildTimeMs);
    Append("ms ");
  }
  if (formatTimeMs != 0) {
    Append("Format: ");
    Append((i32)formatTimeMs);
    Append("ms ");
  }
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
  Append(currentPath);
  Append(" ");
  if (isSaved)
    Append("Saved");
  else
    Append("Modified");

  Append(" ");

  SIZE s;
  const char* ch = "w";
  GetTextExtentPoint32A(dc, ch, 1, &s);

  TextColors(0x888888, 0x000000);
  TextOutA(dc, canvas.width - (strBufferCurrentPos * s.cx) - 20, canvas.height - 30, strBuffer,
           strBufferCurrentPos);
}

f32 lerp(f32 from, f32 to, f32 v) {
  return (1 - v) * from + to * v;
}

void DrawParagraph(i32 x, i32 y, char* text, i32 len) {
  TextColors(0xff2222, 0x000000);
  i32 i = 0;
  while (i < len) {
    i32 lineStart = i;
    while (text[i] != '\n' && i < len)
      i++;

    TextOutA(dc, x, (i32)y, text + lineStart, i - lineStart);

    if (i >= len)
      break;
    i++;
    y += (f32)m.tmHeight * lineHeight;
  }
}

void RenderOutline(HDC dc, Rect rect) {
  if (mode != OutlineInput)
    return;
  TEXTMETRICA m;
  SIZE s;
  GetTextMetrics(dc, &m);
  GetTextExtentPoint32A(dc, "w", 1, &s);

  i32 y = 0;
  if (mode == OutlineInput)
    TextColors(0xffffff, 0x000000);
  else
    TextColors(0x888888, 0x000000);

  TextOut(dc, rect.x - patternLen * s.cx, y, pattern, patternLen);
  y += m.tmHeight;
  i32 currentItemInList = 0;
  for (i32 i = 0; i < entriesCount; i++) {
    char* f = entries[i].name;
    u32 color = 0xff0000;
    char type = entries[i].type;
    if (type == 'f')
      color = 0xDCDCAA;
    if (type == 's' || type == 't')
      color = 0x46C8B1;
    if (type == 'd')
      color = 0xCB5EFF;
    if (type == 'v')
      color = 0xA0DAFB;

    u32 bg = 0x000000;
    if (currentItemInList == selectedOutlineItem)
      bg = 0x666666;

    i32 startX = rect.x - s.cx * strlen(f);
    if (patternLen == 0) {
      TextColors(color, bg);
      TextOut(dc, startX, rect.y + y, f, strlen(f));
      if (!strequal(entries[i].filename, currentPath)) {
        TextColors(0x777777, 0x000000);
        TextOut(dc, rect.x - s.cx * Max(35, strlen(f) + strlen(entries[i].filename) + 1),
                rect.y + y, entries[i].filename, strlen(entries[i].filename));
      }
      y += m.tmHeight;
      currentItemInList++;
    } else {
      i32 index = IndexOf(pattern, entries[i].name);
      if (index >= 0) {

        // first part
        TextColors(color, bg);
        TextOut(dc, startX, rect.y + y, f, index);

        // matching path
        TextColors(0xffffff, bg);
        TextOut(dc, startX + index * s.cx, rect.y + y, f + index, patternLen);

        // right path
        TextColors(color, bg);
        TextOut(dc, startX + (index + patternLen) * s.cx, rect.y + y, f + index + patternLen,
                strlen(f) - (index + patternLen));

        if (!strequal(entries[i].filename, currentPath)) {
          TextColors(0x777777, 0x000000);
          TextOut(dc, rect.x - s.cx * Max(40, strlen(f) + strlen(entries[i].filename) + 1),
                  rect.y + y, entries[i].filename, strlen(entries[i].filename));
        }
        y += m.tmHeight;
        currentItemInList++;
      }
    }
  }
}

void Draw(f32 deltaSec) {
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

  // print selection
  if ((mode == Visual || mode == VisualLine) && selectionStart >= 0) {

    Range range = GetSelectionRange();
    i32 selectionLeft = range.left;
    i32 selectionRight = range.right;

    CursorPos sel1 = GetCursorPos(selectionLeft);
    f32 x1 = padding + (f32)sel1.col * (f32)s.cx;
    f32 y1 = padding - offset.current + sel1.row * m.tmHeight * lineHeight;
    TextColors(0xeeeeee, 0x232D39);

    while (selectionLeft <= selectionRight) {
      i32 lineEnd = Min(FindLineEnd(selectionLeft), selectionRight);

      TextOutA(dc, x1, y1, content + selectionLeft, lineEnd - selectionLeft + 1);
      selectionLeft = lineEnd + 1;
      x1 = padding;
      y1 += m.tmHeight * lineHeight;
    }
  }

  CursorPos p = GetCursorPos(pos);
  u32 cursorbg = 0xFFDC32;
  if (mode == Insert)
    cursorbg = 0xFF3269;
  if (mode == Visual || mode == VisualLine)
    cursorbg = 0xF0EBE6;
  TextColors(0x000000, cursorbg);
  i32 cursorX = x + p.col * s.cx;
  i32 cursorY = padding + p.row * m.tmHeight * lineHeight - offset.current;
  if (mode == Insert)
    PaintRect(cursorX - 2, cursorY, 2, m.tmHeight, cursorbg);
  else {
    PaintRect(cursorX, cursorY, s.cx, m.tmHeight, cursorbg);

    TextOutA(dc, cursorX, cursorY, content + pos, 1);
  }

  PrintFooter(deltaSec * 1000.0f);

  Rect r = {canvas.width / 2, 0, canvas.width / 2, canvas.height};
  if (render) {
    render(dc, &canvas, r, deltaSec);
  } else if (isFailedToBuild) {
    DrawParagraph(r.x, r.y, buildLogs, buildLogsLen);
  }
  if (!render) {
    r.x = canvas.width;
  }
  RenderOutline(dc, r);

  StretchDIBits(windowDC, 0, 0, canvas.width, canvas.height, 0, 0, canvas.width, canvas.height,
                canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

bool IsRunningAnyAnimations() {
  return Abs(offset.current - offset.target) < 0.1;
}

extern "C" void WinMainCRTStartup() {
  PreventWindowsDPIScaling();
  dc = CreateCompatibleDC(0);
  OpenWindow();
  InitAnimations();

  HFONT f = CreateFont(15, "Consolas", FW_REGULAR, CLEARTYPE_NATURAL_QUALITY);
  SelectObject(dc, f);

  GetTextMetrics(dc, &m);

  buildLogs = (char*)valloc(KB(128));
  changes.capacity = KB(40);
  changes.contents = (Change*)valloc(changes.capacity);
  changes.lastChangeIndex = -1;

  windowDC = GetDC(win);

  ReadFileIntoBuffer(files[0]);

  i64 frameStart = GetPerfCounter();
  f32 freq = (f32)GetPerfFrequency();
  f32 lastFrameSec = 0;
  isInitialized = true;
  Draw(lastFrameSec);

  while (isRunning) {
    MSG msg;

    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // TODO: this is naive way not to heat CPU at the start. I will review how I deal with idle
    // state and animations
    if (!IsRunningAnyAnimations()) {
      Draw(lastFrameSec);
      UpdateSpring(&offset, lastFrameSec);
    } else {
      // consider using timeBeginPeriod for better accuracy. Notice that LEAN_AND_MEAN flag excludes
      // timeapi.h
      Sleep(8);
    }

    i64 frameEnd = GetPerfCounter();
    lastFrameSec = (frameEnd - frameStart) / freq;
    frameStart = frameEnd;
  }

  ExitProcess(0);
}
