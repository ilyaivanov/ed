#define WIN32_LEAN_AND_MEAN
#include <stdint.h>
#include <windows.h>

#define KB(v) (v * 1024)
int _fltused = 0x9875;

int _DllMainCRTStartup(HINSTANCE const instance, DWORD const reason, void* const reserved) {
  return 1;
}

typedef int64_t i64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint8_t u8;
typedef int8_t i8;
typedef float f32;

struct Arena {
  char* start;
  i32 size;
  i32 capacity;
};

enum Mode { Normal, Insert };

enum Mode mode = Normal;

typedef struct MyBitmap {
  i32 width;
  i32 height;
  i32 bytesPerPixel;
  u32* pixels;
} MyBitmap;

typedef struct Rect {
  i32 x;
  i32 y;
  i32 width;
  i32 height;
} Rect;

#pragma function(strlen)
size_t strlen(const char* str) {
  i32 res = 0;
  while (str[res] != '\0')
    res++;
  return res;
}

#pragma function(memset)
void* memset(void* dest, int c, size_t count) {
  char* bytes = (char*)dest;
  while (count--) {
    *bytes++ = (char)c;
  }
  return dest;
}

#pragma function(memcpy)
void* memcpy(void* dst, const void* src, size_t n) {
  unsigned char* d = (unsigned char*)dst;
  const unsigned char* s = (const unsigned char*)src;
  while (n--)
    *d++ = *s++;
  return dst;
}

inline void* valloc(size_t size) {
  return VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
};

inline void vfree(void* ptr) {
  VirtualFree(ptr, 0, MEM_RELEASE);
};

int IsPrintable(char ch) {
  return ch >= ' ' && ch <= '}';
}

u32 ToLower(char ch) {
  if (ch >= 'A' && ch <= 'Z')
    return ch + ('a' - 'A');
  return ch;
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

inline BOOL IsKeyPressed(u32 code) {
  return (GetKeyState(code) >> 15) & 1;
}

u32 ToWinColor(u32 color) {
  return ((color & 0xff0000) >> 16) | (color & 0x00ff00) | ((color & 0x0000ff) << 16);
}

void TextColors(HDC dc, u32 foreground, u32 background) {
  SetBkColor(dc, ToWinColor(background));
  SetTextColor(dc, ToWinColor(foreground));
}

void PaintRect(MyBitmap* bitmap, i32 x, i32 y, i32 width, i32 height, u32 color) {
  for (i32 i = x; i < x + width; i++) {
    for (i32 j = y; j < y + height; j++) {
      if (i >= 0 && i < bitmap->width && j < bitmap->height && j >= 0)
        bitmap->pixels[j * bitmap->width + i] = color;
    }
  }
}

void PaintSquareCentered(MyBitmap* bitmap, i32 x, i32 y, i32 size, u32 color) {
  PaintRect(bitmap, x - size / 2, y - size / 2, size, size, color);
}

typedef struct String {
  int len;
  int capacity;
  char content[];
} String;

typedef struct Item {
  String* str;
  struct Item* children[200];
  int childrenLen;
  struct Item* parent;
  int isClosed;
} Item;

struct Arena stringsArena;
String* nextStr;
Item items[200];
int itemsLen;

Item root;
Item* selectedItem;
i32 pos;

typedef struct StackEntry {
  int level;
  Item* item;
} StackEntry;

void AddChildTo(Item* parent, Item* child) {
  parent->children[parent->childrenLen++] = child;
  child->parent = parent;
}

i32 Max(i32 a, i32 b) {
  if (a > b)
    return a;
  return b;
}

Item* CreateItemLen(Item* parent, const char* text, i32 len) {
  if (!nextStr)
    nextStr = (String*)(stringsArena.start + stringsArena.size);

  nextStr->len = len;
  nextStr->capacity = Max(16, len * 2);
  memcpy(&nextStr->content, text, nextStr->len);

  items[itemsLen] = (Item){.str = nextStr};

  stringsArena.size += nextStr->capacity + sizeof(String);
  nextStr = (String*)(stringsArena.start + stringsArena.size);

  Item* res = &items[itemsLen];
  AddChildTo(parent, res);

  itemsLen++;
  return res;
}

Item* CreateItem(Item* parent, const char* text) {
  return CreateItemLen(parent, text, strlen(text));
}

i32 GetChildCount(Item* item) {
  i32 res = item->childrenLen;

  for (i32 i = 0; i < item->childrenLen; i++) {
    if (item->children[i]->childrenLen > 0)
      res += GetChildCount(item->children[i]);
  }

  return res;
}

int hasInit;
void Init() {
  stringsArena.capacity = KB(100);
  stringsArena.start = (char*)valloc(stringsArena.capacity);
  const char* path = "notes.txt";
  i32 size = GetMyFileSize(path);

  char* file = valloc(size);
  ReadFileInto(path, size, file);

  i32 pos = 0;
  StackEntry stack[200] = {-1, &root};
  int stackLen = 1;

  while (pos < size) {

    i32 start = pos;
    while (pos < size && file[pos] != '\n')
      pos++;

    i32 lineStart = start;
    while (file[start] == ' ' && start < pos)
      start++;

    if (pos - start > 0) {
      i32 offset = start - lineStart;
      while (stack[stackLen - 1].level >= offset)
        stackLen--;

      Item* item = CreateItemLen(stack[stackLen - 1].item, file + start, pos - start);
      stack[stackLen].level = offset;
      stack[stackLen++].item = item;
    }
    pos++;
  }

  selectedItem = root.children[0];
}

TEXTMETRICA m;
SIZE s;

i32 FormatNumber(char* buffer, i32 v) {
  i32 l = 0;

  if (v == 0) {
    buffer[l++] = '0';
  }

  while (v > 0) {
    buffer[l++] = '0' + (v % 10);
    v /= 10;
  }

  for (i32 i = 0; i < (l / 2); i++) {
    i32 temp = buffer[i];
    buffer[i] = buffer[l - i - 1];
    buffer[l - i - 1] = temp;
  }
  return l;
}

void PrintNumber(HDC dc, i32 v, i32 x, i32 y, u32 color) {
  char buff[20];
  i32 len = FormatNumber(buff, v);
  TextColors(dc, color, 0);
  TextOut(dc, x - len * s.cx, y, buff, len);
}

void DrawItem(Item* child, MyBitmap* bitmap, HDC dc, int x, int y) {
  u32 squareColor = 0x777777;
  if (child == selectedItem) {
    if (mode == Normal)
      TextColors(dc, 0x99ff99, 0);
    else
      TextColors(dc, 0xff9999, 0);

    squareColor = 0x77aa77;
  } else
    TextColors(dc, 0xffffff, 0);

  i32 squareSize = 6;
  if (child->isClosed)
    PaintSquareCentered(bitmap, x, y + m.tmHeight / 2 + 2, squareSize, squareColor);
  else if (child->childrenLen > 0) {
    PaintRect(bitmap, x + 12 + s.cx / 2 - 1, y + m.tmHeight, 2,
              m.tmHeight * 1.1 * GetChildCount(child), 0x191919);
  }

  TextOut(dc, x + 12, y, child->str->content, child->str->len);
  PrintNumber(dc, (char*)child->str - stringsArena.start, bitmap->width - 5, y, 0x777777);
  if (mode == Insert && child == selectedItem)
    PaintRect(bitmap, x + 12 + s.cx * pos - 1, y, 2, m.tmHeight * 1.1, 0xffaaaa);
}

void AddChildrenToStack(StackEntry* stack, i32* stackLen, Item* parent, i32 level) {
  for (i32 i = parent->childrenLen - 1; i >= 0; i--) {
    stack[*stackLen] = (StackEntry){.level = level, .item = parent->children[i]};
    *stackLen += 1;
  }
}

int IndexOf(Item* item) {
  Item* parent = item->parent;
  for (int i = 0; i < parent->childrenLen; i++) {
    if (parent->children[i] == item)
      return i;
  }
  return -1;
}

int IsLastItem(Item* item) {
  return IndexOf(item) == (item->parent->childrenLen - 1);
}

int IsRoot(Item* item) {
  return item == &root;
}

Item* NextSibling(Item* item) {
  int index = IndexOf(item);
  return item->parent->children[index + 1];
}

void MoveDown() {
  if (selectedItem->childrenLen > 0 && !selectedItem->isClosed)
    selectedItem = selectedItem->children[0];
  else {
    int index = IndexOf(selectedItem);
    if (index < selectedItem->parent->childrenLen - 1)
      selectedItem = selectedItem->parent->children[index + 1];
    else {
      Item* parent = selectedItem->parent;
      while (!IsRoot(parent) && IsLastItem(parent)) {
        parent = parent->parent;
      }
      if (!IsRoot(parent))
        selectedItem = NextSibling(parent);
    }
  }
}

void MoveUp() {
  int index = IndexOf(selectedItem);
  if (index > 0) {
    Item* prev = selectedItem->parent->children[index - 1];
    while (!prev->isClosed && prev->childrenLen > 0)
      prev = prev->children[prev->childrenLen - 1];

    selectedItem = prev;

  } else if (selectedItem->parent != &root) {
    selectedItem = selectedItem->parent;
  }
}

void MoveLeft() {
  if (!selectedItem->isClosed && selectedItem->childrenLen > 0)
    selectedItem->isClosed = 1;
  else if (selectedItem->parent != &root)
    selectedItem = selectedItem->parent;
}

void MoveRight() {
  if (selectedItem->isClosed && selectedItem->childrenLen > 0)
    selectedItem->isClosed = 0;
  else if (selectedItem->childrenLen > 0) {
    selectedItem = selectedItem->children[0];
  }
}

void MoveAllStringsRightBy(Item* item, char* from, i32 by) {
  if (item->str && (char*)item->str > from)
    item->str = (String*)((char*)item->str + by);

  for (i32 i = 0; i < item->childrenLen; i++) {
    MoveAllStringsRightBy(item->children[i], from, by);
  }
}

void InsertCharInto(String* str, i32 pos, char ch) {
  if (str->len == str->capacity) {
    str->capacity *= 2;
    i32 diff = str->capacity - str->len;
    i32 start = (char*)str - stringsArena.start;
    for (i32 i = stringsArena.size - 1; i >= start + diff; i--) {
      stringsArena.start[i + diff] = stringsArena.start[i];
    }
    stringsArena.size += diff;
    MoveAllStringsRightBy(&root, (char*)str, diff);
  }

  for (i32 i = str->len; i > pos; i--) {
    str->content[i] = str->content[i - 1];
  }
  str->content[pos] = ch;
  str->len++;
}

__declspec(dllexport) LRESULT OnLibEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_KEYDOWN) {
    if (mode == Normal) {
      if (wParam == 'J')
        MoveDown();
      if (wParam == 'K')
        MoveUp();
      if (wParam == 'H')
        MoveLeft();
      if (wParam == 'L')
        MoveRight();
      if (wParam == 'I')
        mode = Insert;
    } else if (mode == Insert) {

      if (wParam == VK_ESCAPE || (wParam == 'I' && IsKeyPressed(VK_CONTROL)))
        mode = Normal;
      else if (IsPrintable(wParam)) {
        char ch = wParam;
        if (!IsKeyPressed(VK_SHIFT))
          ch = ToLower(ch);

        InsertCharInto(selectedItem->str, pos, ch);
        pos++;
      }
    }
  }

  return 0;
}

__declspec(dllexport) void GetSome(HDC dc, MyBitmap* bitmap, Rect rect, float d) {

  if (!hasInit) {
    Init();
    hasInit = 1;
  }

  GetTextMetrics(dc, &m);
  GetTextExtentPoint32A(dc, "w", 1, &s);

  i32 step = s.cx * 2;
  i32 x = rect.x + step / 2;
  f32 y = rect.y;
  f32 lineHeightPx = (f32)m.tmHeight * 1.1;

  StackEntry stack[200];
  i32 stackLen = 0;

  AddChildrenToStack(&stack[0], &stackLen, &root, 0);

  while (stackLen > 0) {
    StackEntry entry = stack[stackLen - 1];
    stackLen--;

    DrawItem(entry.item, bitmap, dc, x + entry.level * step, y);
    y += lineHeightPx;

    if (entry.item->childrenLen > 0 && !entry.item->isClosed)
      AddChildrenToStack(&stack[0], &stackLen, entry.item, entry.level + 1);
  }
  PaintRect(bitmap, rect.x - 1, rect.y, 2, rect.height, 0x444444);
}
