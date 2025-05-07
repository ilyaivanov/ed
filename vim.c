#pragma once

#include "string.c"

typedef struct Buffer {
  StringBuffer file;
  i32 cursor;
  i32 selectionStart;
} Buffer;

u32 IsWhitespace(char ch) {
  return ch == ' ' || ch == '\n';
}
u32 IsAlphaNumeric(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}
u32 IsPunctuation(char ch) {
  // use a lookup table
  const char *punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

  const char *p = punctuation;
  while (*p) {
    if (ch == *p) {
      return 1;
    }
    p++;
  }
  return 0;
}
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
  i32 pos = MaxI32(buffer->cursor - 1, 0);
  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  while (pos > 0 && !IsWhitespace(text[pos - 1]))
    pos--;

  return pos;
}

i32 JumpWordBackward(Buffer *buffer) {
  char *text = buffer->file.content;
  i32 pos = MaxI32(buffer->cursor - 1, 0);
  i32 size = buffer->file.size;
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
i32 JumpWordForward(Buffer *buffer) {
  char *text = buffer->file.content;
  i32 pos = buffer->cursor;
  i32 size = buffer->file.size;
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

void InsertCharAt(Buffer *buffer, i32 pos, char ch) {
  i32 textSize = buffer->file.size;
  char *text = currentFile->content;

  memmove(text + pos + 1, text + pos, textSize - pos);

  text[pos] = ch;

  buffer->file.size++;
}

void InsertCharAtCursor(Buffer *buffer, char ch) {
  InsertCharAt(buffer, buffer->cursor, ch);
  buffer->cursor++;
}

i32 GetLineOffset(Buffer *buffer, i32 lineStart) {
  i32 l = lineStart;
  while (buffer->file.content[l] == ' ')
    l++;
  return l - lineStart;
}
void JumpParagraphDown(Buffer *buffer) {
  i32 lineStart = FindLineStart(buffer->cursor);
  i32 initialOffset = GetLineOffset(buffer, lineStart);
  i32 currentOffset = initialOffset;

  while (initialOffset < currentOffset) {
  }
}
void JumpParagraphUp(Buffer *buffer) {}

// TODO: what the fuck is this? what was in my head when I wrote this
i32 GetLineLength(Buffer *text, i32 line) {
  i32 currentLine = 0;
  i32 currentLineLength = 0;
  for (i32 i = 0; i < text->file.size; i++) {
    if (text->file.content[i] == '\n') {
      if (currentLine == line)
        return currentLineLength + 1;

      currentLine++;
      currentLineLength = 0;
    } else {
      currentLineLength++;
    }
  }
  return currentLineLength;
}
