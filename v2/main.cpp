#include "font2.cpp"
#include "items.cpp"
#include "math.cpp"
#include "win32.cpp"

FontData font;

Window window;
f32 lineHeight = 1.2;
i32 fontSize = 14;
u32 color = 0xdddddd;
u32 bg = 0x008800;

Item* selected;

struct StackEntry {
  Item* item;
  i32 level;
};

struct ItemsStack {
  StackEntry items[255];
  i32 size;
};
void SwapSelectedItemDown() {
  i32 index = GetItemIndex(selected);
  if (index < (selected->parent->childrenCount - 1)) {
    Item* tmp = selected->parent->children[index + 1];
    selected->parent->children[index + 1] = selected;
    selected->parent->children[index] = tmp;
  }
}

void MoveDown() {
  Item* nextSelected = GetItemBelow(selected);
  if (nextSelected) {
    if (IsSkippedClosedBracket(nextSelected))
      nextSelected = GetItemBelow(nextSelected);
    if (nextSelected) {
      selected = nextSelected;
    }
  }
}

void MoveLeft() {
  if (!selected->isClosed && selected->childrenCount > 0)
    selected->isClosed = true;
  else if (!IsRoot(selected->parent))
    selected = selected->parent;
}

void MoveRight() {
  if (selected->isClosed)
    selected->isClosed = false;
  else if (selected->childrenCount)
    selected = selected->children[0];
}

void MoveUp() {
  i32 index = GetItemIndex(selected);
  if (index == 0 && !IsRoot(selected->parent))
    selected = selected->parent;
  else if (index != 0) {
    Item* item = GetMostNestedItem(selected->parent->children[index - 1]);
    if (IsSkippedClosedBracket(item))
      item = GetMostNestedItem(item->parent->children[index - 2]);
    if (item)
      selected = item;
  }
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
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN:
    if (wParam == VK_F11) {
      window.isFullscreen = !window.isFullscreen;
      SetFullscreen(window.windowHandle, window.isFullscreen);
    }
    if (wParam == 'Q') {
      PostQuitMessage(0);
      window.isTerminated = 1;
    }
    if (wParam == 'J') {
      if (IsKeyPressed(VK_MENU))
        SwapSelectedItemDown();
      else
        MoveDown();
    }
    if (wParam == 'H')
      MoveLeft();

    if (wParam == 'K')
      MoveUp();
    if (wParam == 'L')
      MoveRight();

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

void PrintLine(i32 x, i32 y, const char* text) {
  while (*text) {
    char ch = *text;
    if (ch >= ' ' && ch <= MAX_CHAR_CODE) {
      CopyMonochromeTextureRectTo(&window.canvas, &font.textures[ch], x, y);
      x += font.charWidth;
    }
    text++;
  }
}

Item root = {"root"};

i32 IsEmptyLine(char* line) {
  while (*line && *line != '\n') {
    if (*line != ' ')
      return 0;
    line++;
  }
  return 1;
}

void Init(char* file) {
  ItemsStack stack = {0};
  stack.size = 1;
  stack.items[0].item = &root;
  stack.items[0].level = -1;

  while (*file) {
    char* start = file;
    while (*file && *file != '\n') {
      file++;
    }

    i32 offset = 0;
    while (start[offset] == ' ')
      offset++;

    // i32 strLen = file - start + 1;
    // char* startStart = file
    char* str = (char*)malloc(file - start + 1);
    memmove(str, start + offset, file - start - offset);
    str[file - start - offset] = '\0';

    if (!IsEmptyLine(start)) {
      i32 level = offset / 2;

      while (stack.items[stack.size - 1].level >= level)
        stack.size--;

      Item* newItem = AddChild(stack.items[stack.size - 1].item, str);
      stack.size++;
      stack.items[stack.size - 1].item = newItem;
      stack.items[stack.size - 1].level = level;
    }

    file++;
  }
  selected = root.children[0];
}

StackEntry Pop(ItemsStack* stack) {
  if (stack->size > 0)
    return stack->items[--stack->size];
  else
    return {0};
}

void Push(ItemsStack* stack, Item* item, i32 level) {
  StackEntry* entry = &stack->items[stack->size];
  entry->item = item;
  entry->level = level;
  stack->size++;
}

void PushItemChilren(ItemsStack* stack, Item* item, i32 level) {
  for (i32 i = item->childrenCount - 1; i >= 0; i--) {
    Push(stack, item->children[i], level);
  }
}

void UpdateAndDraw(f32 deltaSec) {
  i32 x = 20;
  i32 y = 20;
  ItemsStack stack = {0};

  PushItemChilren(&stack, &root, 0);
  while (stack.size > 0) {
    StackEntry entry = Pop(&stack);

    if (IsSkippedClosedBracket(entry.item))
      continue;

    i32 itemX = x + entry.level * (font.charWidth * 2);
    if (entry.item->isClosed)
      PaintRect(V2{(f32)0, (f32)y}, window.canvas.width, font.charHeight * lineHeight, 0x15181e);

    if (selected == entry.item)
      PaintRect(V2{(f32)itemX, (f32)y}, 10, font.charHeight, 0x008800);

    PrintLine(itemX, y, entry.item->title);
    y += font.charHeight * lineHeight;

    if (!entry.item->isClosed)
      PushItemChilren(&stack, entry.item, entry.level + 1);
  }
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

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  PreventWindowsDPIScaling();

  window = {.width = 1400,
            .height = 2100,
            .bg = 0x222222,
            .title = "Editor",
            .isFullscreen = 0,
            .onEvent = OnEvent};

  OpenWindow(&window);

  i64 frameStart = GetPerfCounter();
  i64 appStart = frameStart;
  i64 frameEnd = frameStart;
  f32 freq = (f32)GetPerfFrequency();

  Arena arena = CreateArena(10 * 1024 * 1024);
  font.color = color;
  InitFont(&font, "Consolas", fontSize, &arena);

  i32 size = 0;
  Init(ReadTextFile(".\\foo.cpp", &size));

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
