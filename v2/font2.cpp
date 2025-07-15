#pragma once

#include "math.cpp"
#include "primitives.h"
#include "win32.cpp"

typedef struct Arena {
  u8* start;
  i32 bytesAllocated;
  i32 size;
} Arena;

inline Arena CreateArena(i32 size) {
  Arena res = {0};
  res.size = size;
  res.start = (u8*)VirtualAllocateMemory(res.size);
  return res;
}

u8* ArenaPush(Arena* arena, i32 size) {
  u8* res = arena->start + arena->bytesAllocated;
  arena->bytesAllocated += size;

  // if (arena->bytesAllocated > arena->size)
  //  Fail("Exceeded arena size");

  return res;
}

void ArenaClear(Arena* arena) {
  arena->bytesAllocated = 0;
}

#define MAX_CHAR_CODE 127

void InitBitmapInfo(BITMAPINFO* bitmapInfo, u32 width, u32 height) {
  bitmapInfo->bmiHeader.biSize = sizeof(bitmapInfo->bmiHeader);
  bitmapInfo->bmiHeader.biBitCount = 32;
  bitmapInfo->bmiHeader.biWidth = width;
  bitmapInfo->bmiHeader.biHeight = height; // makes rows go up, instead of going down by default
  bitmapInfo->bmiHeader.biPlanes = 1;
  bitmapInfo->bmiHeader.biCompression = BI_RGB;
}

typedef struct FontData {
  MyBitmap textures[MAX_CHAR_CODE];
  TEXTMETRIC textMetric;
  i32 weight;
  u32 isMonospaced;
  u32 charWidth;
  u32 charHeight;
  u32 color;
} FontData;

// takes dimensions of destinations, reads rect from source at (0,0)
inline void CopyRectTo(MyBitmap* sourceT, MyBitmap* destination) {
  u32* row = (u32*)destination->pixels + destination->width * (destination->height - 1);
  u32* source = (u32*)sourceT->pixels + sourceT->width * (sourceT->height - 1);
  for (i32 y = 0; y < destination->height; y += 1) {
    u32* pixel = row;
    u32* sourcePixel = source;

    for (i32 x = 0; x < destination->width; x += 1) {
      *pixel = *sourcePixel;
      sourcePixel += 1;
      pixel += 1;
    }
    source -= sourceT->width;
    row -= destination->width;
  }
}

MyBitmap fontCanvas = {0};
void InitFont(FontData* fontData, const char* name, i32 fontSize, Arena* arena) {

  HDC deviceContext = CreateCompatibleDC(0);
  if (!deviceContext)
    MessageBox(0, "Failed to create device context", "Error", MB_ICONERROR);

  BITMAPINFO info = {0};
  int textureSize = 256;
  InitBitmapInfo(&info, textureSize, textureSize);

  void* bits;
  HBITMAP fontBitmap = CreateDIBSection(deviceContext, &info, DIB_RGB_COLORS, &bits, 0, 0);
  if (!fontBitmap)
    MessageBox(0, "Failed to create font bitmap", "Error", MB_ICONERROR);
  fontCanvas.height = textureSize;
  fontCanvas.width = textureSize;
  fontCanvas.pixels = (u32*)bits;

  int h = -MulDiv(fontSize, GetDeviceCaps(deviceContext, LOGPIXELSY), USER_DEFAULT_SCREEN_DPI);
  HFONT font = CreateFontA(h, 0, 0, 0,
                           // FW_BOLD,
                           fontData->weight, // Weight
                           0,                // Italic
                           0,                // Underline
                           0,                // Strikeout
                           DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,

                           CLEARTYPE_QUALITY, DEFAULT_PITCH, name);

  if (!font)
    MessageBox(0, "Failed to create font", "Error", MB_ICONERROR);

  SelectObject(deviceContext, fontBitmap);
  SelectObject(deviceContext, font);

  // The high-order byte must be zero
  // https://learn.microsoft.com/en-us/windows/win32/gdi/colorref
  SetBkColor(deviceContext, 0x000000);
  SetTextColor(deviceContext, fontData->color);

  SIZE size;
  // for (wchar_t ch = 'W'; ch < 'W'+1; ch += 1) {
  for (wchar_t ch = ' '; ch < MAX_CHAR_CODE; ch += 1) {
    int len = 1;
    GetTextExtentPoint32W(deviceContext, &ch, len, &size);

    TextOutW(deviceContext, 0, 0, &ch, len);

    MyBitmap* texture = &fontData->textures[ch];
    texture->width = size.cx;
    texture->height = size.cy;

    texture->pixels = (u32*)ArenaPush(arena, texture->height * texture->width * 4);
    if (arena->bytesAllocated >= arena->size)
      MessageBox(0, "Arena is full", "Error", MB_ICONERROR);

    // memmove(texture->pixels, fontCanvas.pixels, texture->width * texture->height * 4);
    CopyRectTo(&fontCanvas, texture);
  }

  GetTextMetricsA(deviceContext, &fontData->textMetric);

  if (fontData->textures['i'].width == fontData->textures['W'].width) {
    fontData->isMonospaced = 1;
    fontData->charWidth = fontData->textures['W'].width;
  }
  fontData->charHeight = fontData->textMetric.tmHeight;

  DeleteObject(font);
  DeleteObject(fontBitmap);
  DeleteDC(deviceContext);
}

inline void CopyMonochromeTextureRectTo(const MyBitmap* canvas, MyBitmap* sourceT, i32 offsetX,
                                        i32 offsetY) {
  u32* row = (u32*)canvas->pixels + offsetX + offsetY * canvas->width;
  u32* source = (u32*)sourceT->pixels + sourceT->width * (sourceT->height - 1);
  for (i32 y = 0; y < sourceT->height; y += 1) {
    u32* pixel = row;
    u32* sourcePixel = source;
    memmove(row, source, sourceT->width * 4);
    source -= sourceT->width;
    row += canvas->width;
  }
}
