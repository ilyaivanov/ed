#pragma once

typedef struct Buffer {
  char *content;
  i32 size;
  i32 capacity;

  i32 cursor;
  i32 selectionStart;
} Buffer;

i32 FindLineStart(Buffer *buffer, i32 pos) {
  while (pos > 0 && buffer->content[pos - 1] != '\n')
    pos--;

  return pos;
}

i32 FindLineEnd(Buffer *buffer, i32 pos) {
  i32 textSize = buffer->size;
  char *text = buffer->content;
  while (pos < textSize && text[pos] != '\n')
    pos++;

  return pos;
}

void RemoveChars(Buffer *string, int from, int to) {
  char *content = string->content;
  int num_to_shift = string->size - (to + 1);

  memmove(content + from, content + to + 1, num_to_shift);

  string->size -= (to - from + 1);
}

void RemoveCharAt(Buffer *buffer, i32 at) {
  RemoveChars(buffer, at, at);
}

Buffer ReadFileIntoDoubledSizedBuffer(char *path) {
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
  Buffer resFile = {.capacity = fileSize * 2, .size = fileSizeAfter, .content = res};

  return resFile;
}
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
  char *text = buffer->content;
  i32 size = buffer->size;
  i32 pos = buffer->cursor;
  while (pos < size && !IsWhitespace(text[pos]))
    pos++;

  while (pos < size && IsWhitespace(text[pos]))
    pos++;

  return pos;
}

i32 JumpWordWithPunctuationBackward(Buffer *buffer) {
  char *text = buffer->content;
  i32 pos = MaxI32(buffer->cursor - 1, 0);
  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  while (pos > 0 && !IsWhitespace(text[pos - 1]))
    pos--;

  return pos;
}

i32 JumpWordBackward(Buffer *buffer) {
  char *text = buffer->content;
  i32 pos = MaxI32(buffer->cursor - 1, 0);
  i32 size = buffer->size;
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
  char *text = buffer->content;
  i32 pos = buffer->cursor;
  i32 size = buffer->size;
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
  i32 textSize = buffer->size;
  char *text = buffer->content;

  memmove(text + pos + 1, text + pos, textSize - pos);

  text[pos] = ch;

  buffer->size++;
}

void InsertCharAtCursor(Buffer *buffer, char ch) {
  InsertCharAt(buffer, buffer->cursor, ch);
  buffer->cursor++;
}

i32 GetLineOffset(Buffer *buffer, i32 lineStart) {
  i32 l = lineStart;
  while (buffer->content[l] == ' ')
    l++;
  return l - lineStart;
}
i32 JumpParagraphDown(Buffer *buffer) {
  i32 res = buffer->cursor + 1;
  while (res <= buffer->size) {
    i32 lineStart = FindLineStart(buffer, res);
    i32 lineEnd = FindLineEnd(buffer, res);
    if (lineStart == lineEnd)
      return res;
    res = lineEnd + 1;
  }
  return res;
}

i32 JumpParagraphUp(Buffer *buffer) {

  i32 res = buffer->cursor - 1;
  while (res > 0){
    i32 lineStart = FindLineStart(buffer, res);
    i32 lineEnd = FindLineEnd(buffer, res);
    if (lineStart == lineEnd)
      return res;
    res = lineStart - 1;
  }
  return res;
}

// TODO: this could be optimized, no need to traverse whole file for each selection line
i32 GetLineLength(Buffer *text, i32 line) {
  i32 currentLine = 0;
  i32 currentLineLength = 0;
  for (i32 i = 0; i < text->size; i++) {
    if (text->content[i] == '\n') {
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
