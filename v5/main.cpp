#include "tree.cpp"
#include "win32.cpp"
#include <malloc.h>

i32 isRunning = 1;

i32 isInitialized = 0;

BITMAPINFO bitmapInfo;
HWND win;
HDC canvasDc;
HDC windowDc;
HFONT font;

HBITMAP canvasBitmap;

MyBitmap canvas;

Item* root;
Item* selectedItem;

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

void Draw() {
  memset(canvas.pixels, 0, canvas.width * canvas.height * 4);

  StackEntry* stack = (StackEntry*)malloc(400 * sizeof(StackEntry));
  i32 stackLen = 0;

  AddChildrenToStack(&stack[0], &stackLen, root, 0);

  u32 textBgColor = 0x000000;
  u32 textColor = 0xeeeeee;
  u32 selectionBgColor = 0x223322;
  u32 selectionColor = 0xffffff;

  SelectObject(canvasDc, font);
  SIZE s;
  GetTextExtentPoint32A(canvasDc, "w", 1, &s);

  TEXTMETRICA m;
  GetTextMetrics(canvasDc, &m);

  f32 lineHeight = 1.1;
  f32 lineHeightPx = m.tmHeight * lineHeight;
  f32 textOffsetY = (lineHeightPx - m.tmHeight) / 2.0f;
  f32 padding = 20;
  f32 y = padding;

  while (stackLen > 0) {
    StackEntry entry = stack[--stackLen];
    Item* item = entry.item;
    i32 level = entry.level;
    f32 x = padding + level * s.cx * 2;

    if (item == selectedItem) {
      PaintRect(&canvas, 0, y, canvas.width, lineHeightPx, 0x223322);
      TextColor(selectionColor);
      BgColor(selectionBgColor);
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

    y += lineHeightPx;
    if (item->childrenLen > 0 && !item->isClosed)
      AddChildrenToStack(&stack[0], &stackLen, item, level + 1);
  }

  StretchDIBits(windowDc, 0, 0, canvas.width, canvas.height, 0, 0, canvas.width, canvas.height,
                canvas.pixels, &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

void LoadFolder([[maybe_unused]] Item* item) {}

void UpdateSelection(Item* item) {
  if (item)
    selectedItem = item;
}

void MoveRight() {
  if (selectedItem->childrenLen == 0 && selectedItem->fileAttrs != 0)
    LoadFolder(selectedItem);
  else if (selectedItem->childrenLen > 0) {
    if (selectedItem->isClosed)
      selectedItem->isClosed = 0;
    else
      selectedItem = selectedItem->children[0];
  }
}

void MoveLeft() {
  if (selectedItem->childrenLen > 0 && !selectedItem->isClosed)
    selectedItem->isClosed = 1;
  else if (!IsRoot(selectedItem->parent))
    selectedItem = selectedItem->parent;
}

LRESULT OnEvent(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_KEYDOWN:
    if (wParam == 'Q') {
      PostQuitMessage(0);
      isRunning = 0;
    }

    if (wParam == 'J')
      UpdateSelection(GetItemBelow(selectedItem));
    if (wParam == 'K')
      UpdateSelection(GetItemAbove(selectedItem));
    if (wParam == 'H')
      MoveLeft();
    if (wParam == 'L')
      MoveRight();

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

int WinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
            [[maybe_unused]] LPSTR lpCmdLine, [[maybe_unused]] int nShowCmd) {

  SetProcessDPIAware();

  canvasDc = CreateCompatibleDC(0);
  win = OpenWindow(OnEvent);
  windowDc = GetDC(win);

  i32 size = 15;
  font = CreateFontf(size, "Consolas", FW_REGULAR, CLEARTYPE_NATURAL_QUALITY);

  root = (Item*)calloc(1, sizeof(Item));

  Item* foo = AddChildAsText(root, L"foo", -1);
  Item* bar = AddChildAsText(root, L"bar", -1);
  AddChildAsText(root, L"buzz", -1);
  AddChildAsText(bar, L"bar child", -1);
  Item* sub = AddChildAsText(bar, L"bar child noather one", -1);
  AddChildAsText(sub, L"sub child 1", -1);
  AddChildAsText(sub, L"sub child 2", -1);
  AddChildAsText(foo, L"foo child", -1);
  AddChildAsText(foo, L"foo child noather one", -1);
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