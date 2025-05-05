#pragma once
#include "string.c"

typedef struct Buffer {
  StringBuffer file;
  i32 cursor;

  i32 cursorLine;
  i32 cursorCol;
} Buffer;

u32 IsWhitespace(char ch) {
  return ch == ' ' || ch == '\n';
}
//;;;;;;;;;; abc ;;;abc;;; abc;;;
i32 JumpWordWithPunctuationForward(StringBuffer *file, i32 currentPos) {
  char *text = file->content;
  i32 pos = currentPos;
  while (pos < file->size && !IsWhitespace(text[pos]))
    pos++;

  while (pos < file->size && IsWhitespace(text[pos]))
    pos++;

  return pos;
}

i32 JumpWordWithPunctuationBackward(StringBuffer *file, i32 currentPos) {
  i32 pos = cursorPos == 0 ? 0 : cursorPos - 1;

  char *text = file->content;
  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  while (pos > 0 && !IsWhitespace(text[pos - 1]))
    pos--;

  return pos;
}


