#pragma once
#include "math.cpp"

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

i32 JumpWordForward(char* text, i32 size, i32 pos) {
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

i32 JumpWordBackward(char* text, i32 size, i32 pos) {
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

i32 JumpWordWithPunctuationForward(char* text, i32 size, i32 pos) {
  while (pos < size && !IsWhitespace(text[pos]))
    pos++;

  while (pos < size && IsWhitespace(text[pos]))
    pos++;

  return pos;
}

i32 JumpWordWithPunctuationBackward(char* text, i32 size, i32 pos) {
  pos = MaxI32(pos - 1, 0);
  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  while (pos > 0 && !IsWhitespace(text[pos - 1]))
    pos--;

  return pos;
}
