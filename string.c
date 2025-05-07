#pragma once
#include "win32.c"

typedef struct StringBuffer {
  char *content;
  i32 size;
  i32 capacity;
} StringBuffer;

i32 FindLineStart(StringBuffer *buffer, i32 pos) {
  while (pos > 0 && buffer->content[pos - 1] != '\n')
    pos--;

  return pos;
}

i32 FindLineEnd(StringBuffer *buffer, i32 pos) {
  i32 textSize = buffer->size;
  char *text = buffer->content;
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

void RemoveCharAt(StringBuffer *buffer, i32 at) {
  RemoveChars(buffer, at, at);
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
