#pragma once
#include "math.cpp"
#include "text.cpp"
#include <stdio.h>

bool IsPrintable(u64 val) {
  return val == 13 || (val >= ' ' && val <= '~');
}

u32 IsWhitespace(char ch) {
  return ch == ' ' || ch == '\n';
}

u32 IsAlphaNumeric(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

u32 IsPunctuation(char ch) {
  // use a lookup table
  const char* punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

  const char* p = punctuation;
  while (*p) {
    if (ch == *p) {
      return 1;
    }
    p++;
  }
  return 0;
}

i32 JumpWordForward(Buffer* buffer) {
  char* text = buffer->file;
  i32 size = buffer->size;
  i32 pos = buffer->cursorPos;
  if (IsWhitespace(text[pos])) {
    while (pos < size && IsWhitespace(text[pos]))
      pos++;
  } else {
    if (IsAlphaNumeric(text[pos])) {
      while (pos < size && IsAlphaNumeric(text[pos]))
        pos++;
    } else {
      while (pos < size && IsPunctuation(text[pos]))
        pos++;
    }
    while (pos < size && IsWhitespace(text[pos]))
      pos++;
  }

  return pos;
}

i32 JumpWordBackward(Buffer* buffer) {
  char* text = buffer->file;
  i32 size = buffer->size;
  i32 pos = buffer->cursorPos;
  pos = MaxI32(pos - 1, 0);
  i32 isStartedAtWhitespace = IsWhitespace(text[pos]);

  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  if (IsAlphaNumeric(text[pos])) {
    while (pos > 0 && IsAlphaNumeric(text[pos]))
      pos--;
  } else {
    while (pos > 0 && IsPunctuation(text[pos]))
      pos--;
  }
  pos++;

  if (!isStartedAtWhitespace) {
    while (pos > 0 && IsWhitespace(text[pos]))
      pos--;
  }

  return pos;
}

i32 JumpWordForwardIgnorePunctuation(Buffer* buffer) {
  char* text = buffer->file;
  i32 size = buffer->size;
  i32 pos = buffer->cursorPos;
  while (pos < size && !IsWhitespace(text[pos]))
    pos++;

  while (pos < size && IsWhitespace(text[pos]))
    pos++;

  return pos;
}

i32 JumpWordBackwardIgnorePunctuation(Buffer* buffer) {
  char* text = buffer->file;
  i32 size = buffer->size;
  i32 pos = buffer->cursorPos;
  pos = MaxI32(pos - 1, 0);
  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  while (pos > 0 && !IsWhitespace(text[pos - 1]))
    pos--;

  return pos;
}

i32 JumpToTheEndOfWordIgnorePunctuation(Buffer* buffer) {
  i32 pos = buffer->cursorPos;
  char* text = buffer->file;
  i32 size = buffer->size;
  pos++;

  while (pos < size && IsWhitespace(text[pos]))
    pos++;

  while (pos < size && !IsWhitespace(text[pos]))
    pos++;

  return pos - 1;
}

i32 JumpToTheEndOfWord(Buffer* buffer) {
  i32 pos = buffer->cursorPos;
  char* text = buffer->file;
  i32 size = buffer->size;
  pos++;

  while (pos < size && IsWhitespace(text[pos]))
    pos++;

  if (IsAlphaNumeric(text[pos])) {
    while (IsAlphaNumeric(text[pos]))
      pos++;
  } else {

    while (!IsAlphaNumeric(text[pos]))
      pos++;
  }

  return pos - 1;
}

i32 FindLineEnd(Buffer* buffer, i32 pos) {
  while (pos < buffer->size && buffer->file[pos] != '\n')
    pos++;

  return pos;
}

i32 FindIndexOfCharForward(Buffer* buffer, i32 from, char ch) {
  while (from < buffer->size) {
    if (buffer->file[from] == ch)
      return from;
    from++;
  }
  return -1;
}

i32 FindIndexOfCharBackward(Buffer* buffer, i32 from, char ch) {
  while (from >= 0) {
    if (buffer->file[from] == ch)
      return from;
    from--;
  }
  return -1;
}

i32 FindLineStart(Buffer* buffer, i32 pos) {
  while (pos > 0 && buffer->file[pos - 1] != '\n')
    pos--;

  return pos;
}

i32 GetOffsetForLineAt(Buffer* buffer, i32 pos) {
  i32 start = FindLineStart(buffer, pos);
  i32 p = start;
  while (buffer->file[p] == ' ')
    p++;
  return p - start;
}
