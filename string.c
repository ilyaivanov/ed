#pragma once
#include "win32.c"

typedef struct StringBuffer {
  char *content;
  i32 size;
  i32 capacity;
} StringBuffer;

StringBuffer *currentFile;

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

void RemoveChars(StringBuffer *string, int from, int to) {
  // Calculate the number of characters to shift
  char *content = string->content;
  int num_to_shift = string->size - (to + 1);

  // Use memmove to shift the remaining characters
  memmove(content + from, content + to + 1, num_to_shift);

  // Update size after removing the characters
  string->size -= (to - from + 1);
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
