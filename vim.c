#pragma once
#include "string.c"

typedef struct Buffer {
  StringBuffer file;
  i32 cursor;
} Buffer;

u32 IsWhitespace(char ch) {
  return ch == ' ' || ch == '\n';
}
//;;;;;;;;;; abc ;;;abc;;; abc;;;
i32 JumpWordWithPunctuationForward(Buffer *buffer) {
  char *text = buffer->file.content;
  i32 size = buffer->file.size;
  i32 pos = buffer->cursor;
  while (pos < size && !IsWhitespace(text[pos]))
    pos++;

  while (pos < size && IsWhitespace(text[pos]))
    pos++;

  return pos;
}

i32 JumpWordWithPunctuationBackward(Buffer *buffer) {
  char *text = buffer->file.content;
  i32 pos = buffer->cursor;
  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  while (pos > 0 && !IsWhitespace(text[pos - 1]))
    pos--;

  return pos;
}

void InsertCharAtCursor(Buffer *buffer, char ch) {
  i32 textSize = buffer->file.size;
  i32 pos = buffer->cursor;
  char *text = currentFile->content;

  memmove(text + pos + 1, text + pos, textSize - pos);

  text[pos] = ch;

  buffer->file.size++;
  buffer->cursor++;
}
