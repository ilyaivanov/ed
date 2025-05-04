#pragma once
#include "win32.c"

typedef struct StringBuffer {
  char *content;
  i32 size;
  i32 capacity;
} StringBuffer;

StringBuffer *currentFile;
i32 cursorPos = 0;

inline void MoveBytesLeft(char *ptr, int length) {
  for (int i = 0; i < length - 1; i++) {
    ptr[i] = ptr[i + 1];
  }
}

inline void PlaceLineEnd() {
  if (currentFile->content)
    *(currentFile->content + currentFile->size) = '\0';
}

void RemoveCharAt(i32 at) {
  MoveBytesLeft(currentFile->content + at, currentFile->size - at);
  currentFile->size--;
  PlaceLineEnd();
}

void InsertCharAtCursor(char ch) {
  i32 textSize = currentFile->size;
  i32 pos = cursorPos;
  char *text = currentFile->content;
  memmove(text + pos + 1, text + pos, textSize - pos);

  text[pos] = ch;

  currentFile->size++;
  cursorPos++;
}

i32 FindLineStart(i32 pos) {
  while (pos > 0 && currentFile->content[pos - 1] != '\n')
    pos--;

  return pos;
}

i32 FindLineEnd(i32 pos) {
  i32 textSize = currentFile->size;
  char *text = currentFile->content;
  while (pos < textSize && text[pos] != '\n')
    pos++;

  return pos;
}

StringBuffer ReadFileIntoDoubledSizedBuffer(char *path) {
  u32 fileSize = GetMyFileSize(path);
  char *file = VirtualAllocateMemory(fileSize);
  ReadFileInto(path, fileSize, file);

  char *res = VirtualAllocateMemory(fileSize * 2);
  int fileSizeAfter = 0;
  for (i32 i = 0; i < fileSize; i++) {
    if (file[i] != '\r')
      res[fileSizeAfter++] = file[i];
  }
  VirtualFreeMemory(file);
  StringBuffer resFile = {.capacity = fileSize * 2, .size = fileSizeAfter, .content = res};

  return resFile;
}
