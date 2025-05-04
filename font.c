
#pragma once

#include "win32.c"

typedef struct Arena {
  u8 *start;
  i32 bytesAllocated;
  i32 size;
} Arena;

inline Arena CreateArena(i32 size) {
  Arena res = {0};
  res.size = size;
  res.start = (u8 *)VirtualAllocateMemory(res.size);
  return res;
}

u8 *ArenaPush(Arena *arena, i32 size) {
  u8 *res = arena->start + arena->bytesAllocated;
  arena->bytesAllocated += size;

  //if (arena->bytesAllocated > arena->size)
   // Fail("Exceeded arena size");

  return res;
}

void ArenaClear(Arena *arena) { arena->bytesAllocated = 0; }

#define MAX_CHAR_CODE 126

void InitBitmapInfo(BITMAPINFO *bitmapInfo, u32 width, u32 height) {
  bitmapInfo->bmiHeader.biSize = sizeof(bitmapInfo->bmiHeader);
  bitmapInfo->bmiHeader.biBitCount = 32;
  bitmapInfo->bmiHeader.biWidth = width;
  bitmapInfo->bmiHeader.biHeight =
      height; // makes rows go up, instead of going down by default
  bitmapInfo->bmiHeader.biPlanes = 1;
  bitmapInfo->bmiHeader.biCompression = BI_RGB;
}

typedef struct MonochromeTexture {
  i32 width;
  i32 height;
  u8 *pixels;
} MonochromeTexture;
typedef struct FontData {
  MonochromeTexture textures[MAX_CHAR_CODE];
  TEXTMETRIC textMetric;
  u32 isMonospaced;
  u32 charWidth;
  u32 charHeight;
} FontData;

// takes dimensions of destinations, reads rect from source at (0,0)
inline void CopyRectTo(MyBitmap *sourceT, MonochromeTexture *destination) {
  u8 *row = (u8 *)destination->pixels +
            destination->width * (destination->height - 1);
  u32 *source = (u32 *)sourceT->pixels + sourceT->width * (sourceT->height - 1);
  for (i32 y = 0; y < destination->height; y += 1) {
    u8 *pixel = row;
    u32 *sourcePixel = source;
    for (i32 x = 0; x < destination->width; x += 1) {
      u32 alpha = (*sourcePixel & 0xff) << 24;
      *pixel = (u8)(*sourcePixel | alpha);
      sourcePixel += 1;
      pixel += 1;
    }
    source -= sourceT->width;
    row -= destination->width;
  }
}


void InitFont(FontData *fontData, char *name, i32 fontSize, Arena *arena) {

  HDC deviceContext = CreateCompatibleDC(0);
  BITMAPINFO info = {0};
  int textureSize = 256;
  InitBitmapInfo(&info, textureSize, textureSize);

  void *bits;
  HBITMAP fontBitmap =
      CreateDIBSection(deviceContext, &info, DIB_RGB_COLORS, &bits, 0, 0);
  MyBitmap fontCanvas = {0};
  fontCanvas.height = textureSize;
  fontCanvas.width = textureSize;
  fontCanvas.pixels = (u32 *)bits;

  int h = -MulDiv(fontSize, GetDeviceCaps(deviceContext, LOGPIXELSY),
                  USER_DEFAULT_SCREEN_DPI);
  HFONT font =
      CreateFontA(h, 0, 0, 0,
                  FW_DONTCARE, // Weight
                  0,           // Italic
                  0,           // Underline
                  0,           // Strikeout
                  DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS,

                  PROOF_QUALITY, DEFAULT_PITCH, name);

  SelectObject(deviceContext, fontBitmap);
  SelectObject(deviceContext, font);

  // The high-order byte must be zero
  // https://learn.microsoft.com/en-us/windows/win32/gdi/colorref
  SetBkColor(deviceContext, 0x000000);
  SetTextColor(deviceContext, 0xffffff);

  SIZE size;
  for (wchar_t ch = ' '; ch < MAX_CHAR_CODE; ch += 1) {
    int len = 1;
    GetTextExtentPoint32W(deviceContext, &ch, len, &size);

    TextOutW(deviceContext, 0, 0, &ch, len);

    MonochromeTexture *texture = &fontData->textures[ch];
    texture->width = size.cx;
    texture->height = size.cy;

    texture->pixels = (u8 *)ArenaPush(arena, texture->height * texture->width);

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

inline u32 AlphaBlendColors(u32 from, u32 to, f32 factor){
  u8 fromR = (u8)((from & 0xff0000) >> 16);
  u8 fromG = (u8)((from & 0x00ff00) >> 8);
  u8 fromB = (u8)((from & 0x0000ff) >> 0);

  u8 toR = (u8)((to & 0xff0000) >> 16);
  u8 toG = (u8)((to & 0x00ff00) >> 8);
  u8 toB = (u8)((to & 0x0000ff) >> 0);

  u8 blendedR = RoundU8(lerp(fromR, toR, factor));
  u8 blendedG = RoundU8(lerp(fromG, toG, factor));
  u8 blendedB = RoundU8(lerp(fromB, toB, factor));

  return (blendedR << 16) | (blendedG << 8) | (blendedB << 0);
}
inline u32 AlphaBlendGreyscale(u32 destination, u8 source, u32 color) {
  u8 destR = (u8)((destination & 0xff0000) >> 16);
  u8 destG = (u8)((destination & 0x00ff00) >> 8);
  u8 destB = (u8)((destination & 0x0000ff) >> 0);

  u8 sourceR = (u8)((color & 0xff0000) >> 16);
  u8 sourceG = (u8)((color & 0x00ff00) >> 8);
  u8 sourceB = (u8)((color & 0x0000ff) >> 0);

  f32 a = ((f32)source) / 255.0f;
  u8 blendedR = RoundU8(lerp(destR, sourceR, a));
  u8 blendedG = RoundU8(lerp(destG, sourceG, a));
  u8 blendedB = RoundU8(lerp(destB, sourceB, a));

  return (blendedR << 16) | (blendedG << 8) | (blendedB << 0);
}

inline void CopyMonochromeTextureRectTo(const MyBitmap *canvas,
                                        const Rect *rect,
                                        MonochromeTexture *sourceT, i32 offsetX,
                                        i32 offsetY, u32 color) {
  u32 *row = (u32 *)canvas->pixels + offsetX + offsetY * canvas->width;
  u8 *source = (u8 *)sourceT->pixels + sourceT->width * (sourceT->height - 1);
  for (i32 y = 0; y < sourceT->height; y += 1) {
    u32 *pixel = row;
    u8 *sourcePixel = source;
    for (i32 x = 0; x < sourceT->width; x += 1) {
      // stupid fucking logic needs to extracted outside of the loop
      if (*sourcePixel != 0 && (y + offsetY) > rect->y &&
          (x + offsetX) > rect->x && (x + offsetX) < (rect->x + rect->width) &&
          (y + offsetY) < (rect->y + rect->height))
        // *pixel = *sourcePixel;
        *pixel = AlphaBlendGreyscale(*pixel, *sourcePixel, color);

      sourcePixel += 1;
      pixel += 1;
    }
    source -= sourceT->width;
    row += canvas->width;
  }
}
