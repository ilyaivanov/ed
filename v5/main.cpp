#include "tree.cpp"
#include "win32.cpp"

#define WEIRD_NUMBER 200

u32 textBgColor = 0x000000;
u32 textColor = 0xdddddd;

u32 selectionBgColor = 0x112011;
u32 selectionColor = 0xffffff;
u32 cursorColor = 0x228822;

u32 selectionBgColorInsert = 0x201111;
u32 selectionColorInsert = 0xffffff;
u32 cursorColorInsert = 0xff2222;

f32 vPadding = 20;

i32 isRunning = 1;

f32 lineHeight = 1.1;
i32 fontSize = 15;
i32 isInitialized = 0;

enum Mode { Normal, Insert };
Mode mode;

BITMAPINFO bitmapInfo;
HWND win;
HDC canvasDc;
HDC windowDc;

u32 fontQuality = CLEARTYPE_NATURAL_QUALITY;
// u32 fontQuality = ANTIALIASED_QUALITY;
const char* fontName = "Consolas";
HFONT font;

f32 offset;

SIZE s;
TEXTMETRICW m;

HBITMAP canvasBitmap;

MyBitmap canvas;

Item* root;
Item* selectedItem;
i32 cursorPos;
i32 desiredOffset;

c16 rootPath[MAX_PATH];

void Size(LPARAM lParam) {
  canvas.width = LOWORD(lParam);
  canvas.height = HIWORD(lParam);
  InitBitmapInfo(&bitmapInfo, canvas.width, canvas.height);
  if (canvasBitmap)
    DeleteObject(canvasBitmap);

  canvasBitmap =
      CreateDIBSection(canvasDc, &bitmapInfo, DIB_RGB_COLORS, (void**)&canvas.pixels, 0, 0);
  canvas.bytesPerPixel = 4;

  SelectObject(canvasDc, canvasBitmap);
}

HFONT CreateFontf(i32 fontSize, const char* name, i32 weight, i32 quiality) {
  int h = -MulDiv(fontSize, GetDeviceCaps(canvasDc, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
  return CreateFontA(h, 0, 0, 0,
                     // FW_BOLD,
                     weight,
                     0, // Italic
                     0, // Underline
                     0, // Strikeout
                     DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, quiality,
                     DEFAULT_PITCH, name);
}

void TextColor(u32 color) {
  SetTextColor(canvasDc, ToWinColor(color));
}

void BgColor(u32 color) {
  SetBkColor(canvasDc, ToWinColor(color));
}

f32 GetItemOffsetFromTop(Item* item) {
  Item* current = item;
  i32 len = 0;
  while (current) {
    current = GetItemAbove(current);
    len++;
  }

  return vPadding + (f32)len * m.tmHeight * lineHeight;
}

void CenterViewOnItem() {
  offset = GetItemOffsetFromTop(selectedItem) - (f32)canvas.height / 2.0f;
}

void Draw() {
  memset(canvas.pixels, 0, canvas.width * canvas.height * 4);

  StackEntry* stack = (StackEntry*)malloc(400 * sizeof(StackEntry));
  i32 stackLen = 0;

  AddChildrenToStack(&stack[0], &stackLen, root, 0);

  SelectObject(canvasDc, font);
  GetTextExtentPoint32W(canvasDc, L"w", 1, &s);

  GetTextMetricsW(canvasDc, &m);

  f32 lineHeightPx = m.tmHeight * lineHeight;
  f32 textOffsetY = (lineHeightPx - m.tmHeight) / 2.0f;
  f32 hPadding = s.cx * 3;
  f32 y = -offset + vPadding;

  while (stackLen > 0) {
    StackEntry entry = stack[--stackLen];
    Item* item = entry.item;
    i32 level = entry.level;
    f32 x = hPadding + level * s.cx * 2;

    if (item == selectedItem) {
      u32 lineColor = mode == Insert ? selectionBgColorInsert : selectionBgColor;

      PaintRect(&canvas, 0, y, canvas.width, lineHeightPx, lineColor);

      BgColor(lineColor);
      if (mode == Insert)
        TextColor(selectionColorInsert);
      else
        TextColor(selectionColor);

    } else {
      TextColor(textColor);
      BgColor(textBgColor);
    }

    i32 squareSize = 6;
    u32 squareColor = 0x555555;
    if (item->isClosed)
      PaintSquareCentered(&canvas, x - s.cx * 1.5, y + m.tmHeight / 2 + 2, squareSize, squareColor);
    else if (item->childrenLen > 0) {
      PaintRect(&canvas, x + s.cx * 0.5f - 1, y + m.tmHeight, 2,
                m.tmHeight * lineHeight * GetVisibleChildCount(item), 0x191919);
    }

    TextOutW(canvasDc, x, y + textOffsetY, item->text, item->textLen);

    if (item == selectedItem) {
      if (mode == Insert) {
        PaintRect(&canvas, x + cursorPos * s.cx - 1, y, 2, lineHeightPx, cursorColorInsert);
      } else {
        BgColor(cursorColor);

        // this -1 and +2 is due to TextOutW drawing an outline wider by 1 pixel. I want to have a
        // rectangular cursor of lineHeight, not oval this seems to be regardless of fontSize
        // (tested on 50 fontsize)
        PaintRect(&canvas, x + cursorPos * s.cx - 1, y, s.cx + 2, lineHeightPx, cursorColor);

        if (item->textLen > 0 && cursorPos < item->textLen)
          TextOutW(canvasDc, x + cursorPos * s.cx, y + textOffsetY, item->text + cursorPos, 1);
      }
    }
    y += lineHeightPx;
    if (item->childrenLen > 0 && !item->isClosed)
      AddChildrenToStack(&stack[0], &stackLen, item, level + 1);
  }

  // Scrollbar
  //
  f32 pageHeight = GetVisibleChildCount(root) * m.tmHeight * lineHeight;
  f32 height = canvas.height;

  if (pageHeight > height) {
    i32 scrollHeight = height * height / pageHeight;

    f32 maxOffset = pageHeight - height;
    f32 maxScrollY = height - scrollHeight;
    f32 scrollY = lerp(0, maxScrollY, offset / maxOffset);

    i32 scrollWidth = 10;
    PaintRect(&canvas, canvas.width - scrollWidth, scrollY, scrollWidth, scrollHeight, 0x555555);
  }

  StretchDIBits(windowDc, 0, 0, canvas.width, canvas.height, 0, 0, canvas.width, canvas.height,
                canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

i32 Appendw(c16* buff, i32 len, const c16* str) {
  i32 i = 0;
  while (str[i] != 0) {
    buff[len] = str[i];
    len++;
    i++;
  }

  return len;
}

i32 FillItemPath(c16* buff, Item* item) {
  Item* path[20] = {0};
  i32 pathLen = 0;

  Item* current = item;

  while (!IsRoot(current)) {
    path[pathLen++] = current;
    current = current->parent;
  }

  i32 len = 0;
  len = Appendw(buff, len, rootPath);
  for (i32 i = pathLen - 1; i >= 0; i--) {
    len = Appendw(buff, len, L"\\");
    len = Appendw(buff, len, path[i]->text);
  }

  return len;
}

int IsFileFiltered(WIN32_FIND_DATAW data) {
  i32 len = wcslen(data.cFileName);
  return !(data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) &&
         !(data.cFileName[0] == L'.' && len == 1) &&
         !(data.cFileName[0] == L'.' && data.cFileName[1] == L'.' && len == 2);
}

int FindIndexOfLastFolder(Item* item) {
  for (i32 i = 0; i < item->childrenLen; i++) {
    if (!IsFolder(item->children[i]))
      return i;
  }
  return item->childrenLen;
}

void TryAddItem(WIN32_FIND_DATAW data, Item* parent) {
  if (IsFileFiltered(data)) {
    i32 index = -1;
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      index = FindIndexOfLastFolder(parent);

    Item* item = AddChildAsText(parent, data.cFileName, index);
    item->fileAttrs = data.dwFileAttributes;
  }
}

void LoadFolder(Item* item) {
  c16 path[MAX_PATH] = {0};
  i32 len = FillItemPath(path, item);

  if (IsFolder(item) || IsRoot(item)) {
    len = Appendw(path, len, L"\\*");
    WIN32_FIND_DATAW data;
    HANDLE file = FindFirstFileW(path, &data);
    if (file) {

      TryAddItem(data, item);

      while (FindNextFileW(file, &data)) {
        TryAddItem(data, item);
      }
    }
  } else {
    i32 size = GetMyFileSize(path);

    char* file = (char*)valloc(size);
    ReadFileInto(path, size, file);

    i32 pos = 0;
    StackEntry stack[WEIRD_NUMBER] = {{-1, item}};
    int stackLen = 1;

    while (pos < size) {
      i32 start = pos;
      while (pos < size && file[pos] != '\n')
        pos++;

      i32 lineStart = start;
      while (file[start] == ' ' && start < pos)
        start++;

      i32 offset = start - lineStart;

      // special case for empty lines
      if (pos - start == 0) {
        offset = stack[stackLen - 1].level;
      }

      while (stack[stackLen - 1].level >= offset)
        stackLen--;

      Item* item = CreateItemLen(stack[stackLen - 1].item, file + start, pos - start);
      stack[stackLen].level = offset;
      stack[stackLen++].item = item;

      pos++;
    }
    vfree(file);
  }
}

void UpdateCursorPos(i32 pos) {
  cursorPos = Max(Min(pos, selectedItem->textLen), 0);
}

void UpdateSelection(Item* item) {
  if (item) {
    selectedItem = item;
    UpdateCursorPos(cursorPos);
    f32 itemOffset = GetItemOffsetFromTop(item);
    if (itemOffset < offset)
      offset = itemOffset - m.tmHeight * 2;
    else if (itemOffset > offset + canvas.height)
      offset = itemOffset - canvas.height + m.tmHeight * 2;
  }
}

void MoveDown() {
  UpdateSelection(GetItemBelow(selectedItem));
}

void MoveUp() {
  UpdateSelection(GetItemAbove(selectedItem));
}

void MoveToChild() {
  if (selectedItem->childrenLen == 0 && selectedItem->fileAttrs != 0)
    LoadFolder(selectedItem);
  else if (selectedItem->childrenLen > 0) {
    if (selectedItem->isClosed)
      selectedItem->isClosed = 0;
    else
      UpdateSelection(selectedItem->children[0]);
  }
}

void MoveLeft() {
  if (selectedItem->childrenLen > 0 && !selectedItem->isClosed)
    selectedItem->isClosed = 1;
  else if (!IsRoot(selectedItem->parent))
    UpdateSelection(selectedItem->parent);
}

void MoveToNextSibling() {
  UpdateSelection(GetNextSibling(selectedItem));
}

void MoveToPrevSibling() {
  UpdateSelection(GetPrevSibling(selectedItem));
}

void MoveToParent() {
  if (!IsRoot(selectedItem->parent))
    UpdateSelection(selectedItem->parent);
}

void OpenAllSiblings() {
  Item* parent = selectedItem->parent;
  for (i32 i = 0; i < parent->childrenLen; i++) {
    Item* item = parent->children[i];
    if (item->childrenLen > 0)
      item->isClosed = 0;
  }
}

void CloseAllSiblings() {
  Item* parent = selectedItem->parent;
  for (i32 i = 0; i < parent->childrenLen; i++) {
    Item* item = parent->children[i];
    if (item->childrenLen > 0)
      item->isClosed = 1;
  }
}

void RemoveCharFromLeft() {
  if (cursorPos > 0) {
    RemoveChars(selectedItem, cursorPos - 1, cursorPos - 1);
    cursorPos -= 1;
  }
}

void RemoveWordFromLeft() {
  if (cursorPos > 0) {
    i32 wordEnd = cursorPos - 1;
    i32 wordStart = wordEnd;
    if (IsWhitespace(selectedItem->text[wordStart])) {
      while (wordStart > 0 && IsWhitespace(selectedItem->text[wordStart]))
        wordStart--;

    } else {
      while (wordStart > 0 && !IsWhitespace(selectedItem->text[wordStart]))
        wordStart--;
    }

    RemoveChars(selectedItem, wordStart, wordEnd);
    cursorPos -= wordEnd - wordStart + 1;
  }
}

void RemoveCurrentChar() {
  if (cursorPos < selectedItem->textLen) {
    RemoveChars(selectedItem, cursorPos, cursorPos);
    UpdateCursorPos(cursorPos);
  }
}

void CreateItemBefore() {
  Item* child = (Item*)calloc(1, sizeof(Item));
  AddChildAt(selectedItem->parent, child, IndexOf(selectedItem));
  selectedItem = child;
  cursorPos = 0;
  mode = Insert;
}

void CreateItemAfter() {
  Item* child = (Item*)calloc(1, sizeof(Item));
  AddChildAt(selectedItem->parent, child, IndexOf(selectedItem) + 1);
  selectedItem = child;
  cursorPos = 0;
  mode = Insert;
}

i32 ignoreNextCharEvent = 0;
LRESULT OnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_CHAR:
    if (ignoreNextCharEvent) {
      ignoreNextCharEvent = 0;
      return 0;
    }

    if (mode == Insert) {
      InsertCharAtCursor(selectedItem, cursorPos, wParam);
      cursorPos++;
    }

    break;
  case WM_KEYDOWN:
    if (mode == Insert) {
      if (wParam == VK_ESCAPE) {
        mode = Normal;
        ignoreNextCharEvent = 1;
      } else if (wParam == VK_BACK && IsKeyPressed(VK_CONTROL)) {
        RemoveWordFromLeft();
        ignoreNextCharEvent = 1;
      } else if (wParam == VK_BACK) {
        RemoveCharFromLeft();
        ignoreNextCharEvent = 1;
      }
    }
    if (mode == Normal) {
      if (wParam == 'Q') {
        PostQuitMessage(0);
        isRunning = 0;
      }
      if (wParam == 'W')
        UpdateCursorPos(JumpWordForward(selectedItem, cursorPos));
      if (wParam == 'B')
        UpdateCursorPos(JumpWordBackward(selectedItem, cursorPos));

      if (wParam == 'O' && IsKeyPressed(VK_CONTROL))
        OpenAllSiblings();

      if (wParam == 'Y' && IsKeyPressed(VK_CONTROL))
        CloseAllSiblings();

      if (IsKeyPressed(VK_CONTROL)) {
        if (wParam == 'J')
          MoveToNextSibling();
        if (wParam == 'K')
          MoveToPrevSibling();
        if (wParam == 'H')
          MoveLeft();
        if (wParam == 'L')
          MoveToChild();
      } else {
        if (wParam == 'J')
          MoveDown();
        if (wParam == 'K')
          MoveUp();
        if (wParam == 'H')
          UpdateCursorPos(cursorPos - 1);
        if (wParam == 'L')
          UpdateCursorPos(cursorPos + 1);
      }

      if (wParam == VK_BACK && IsKeyPressed(VK_CONTROL))
        RemoveWordFromLeft();
      else if (wParam == VK_BACK)
        RemoveCharFromLeft();
      if (wParam == 'X')
        RemoveCurrentChar();

      if (wParam == 'Z')
        CenterViewOnItem();
      if (wParam == 'O' && IsKeyPressed(VK_SHIFT))
        CreateItemBefore();
      else if (wParam == 'O')
        CreateItemAfter();

      if (wParam == 'I' && IsKeyPressed(VK_SHIFT)) {
        mode = Insert;
        cursorPos = 0;
      } else if (wParam == 'I')
        mode = Insert;
      else if (wParam == 'A' && IsKeyPressed(VK_SHIFT)) {
        mode = Insert;
        UpdateCursorPos(selectedItem->textLen);
      } else if (wParam == 'A') {
        mode = Insert;
        UpdateCursorPos(cursorPos + 1);
      }
      ignoreNextCharEvent = 1;
    }

    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    isRunning = 0;
    break;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    BeginPaint(handle, &ps);
    EndPaint(handle, &ps);
  } break;
  case WM_SIZE:
    Size(lParam);
    if (isInitialized)
      Draw();
    break;
  }
  return DefWindowProc(handle, message, wParam, lParam);
}

int wWinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
             [[maybe_unused]] PWSTR lpCmdLine, [[maybe_unused]] int nShowCmd) {

  SetProcessDPIAware();

  GetCurrentDirectoryW(MAX_PATH, (wchar_t*)&rootPath[0]);

  canvasDc = CreateCompatibleDC(0);

  win = OpenWindow(OnEvent);
  windowDc = GetDC(win);

  font = CreateFontf(fontSize, fontName, FW_REGULAR, fontQuality);

  root = (Item*)calloc(1, sizeof(Item));

  LoadFolder(root);

  selectedItem = root->children[0];

  isInitialized = 1;
  while (isRunning) {
    MSG msg;

    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    Draw();
  }

  ExitProcess(0);
}