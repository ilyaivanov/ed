#pragma once

typedef enum ChangeType { Added, Removed } ChangeType;

typedef struct Change {
  ChangeType type;
  int groupId;
  int at;
  int size;
  char text[];
} Change;

typedef struct ChangeArena {
  Change* contents;
  int capacity;
  int changesCount;
} ChangeArena;

typedef struct Buffer {
  char* content;
  i32 size;
  i32 capacity;

  i32 cursor;
  i32 selectionStart;

  i32 isSaved;
  char filePath[512];
  int currentChangeGroup;
  ChangeArena changeArena;
} Buffer;

#define ChangeSize(c) (sizeof(Change) + c->size)
#define NextChange(c) ((Change*)(((u8*)c) + ChangeSize(c)))

int GetChangeArenaSize(ChangeArena* arena) {
  Change* c = arena->contents;
  int res = 0;
  for (int i = 0; i < arena->changesCount; i++) {
    res += ChangeSize(c);
    c = NextChange(c);
  }
  return res;
}

i32 FindLineStart(Buffer* buffer, i32 pos) {
  while (pos > 0 && buffer->content[pos - 1] != '\n')
    pos--;

  return pos;
}

i32 FindLineEnd(Buffer* buffer, i32 pos) {
  i32 textSize = buffer->size;
  char* text = buffer->content;
  while (pos < textSize && text[pos] != '\n')
    pos++;

  return pos;
}

void DoubleCapacityIfFull(Buffer* buffer) {
  char* currentStr = buffer->content;
  buffer->capacity = (buffer->capacity == 0) ? 4 : (buffer->capacity * 2);
  buffer->content = VirtualAllocateMemory(buffer->capacity);
  if (currentStr) {
    memmove(buffer->content, currentStr, buffer->size);
    VirtualFreeMemory(currentStr);
  }
}

void BufferRemoveChars(Buffer* string, int from, int to) {
  char* content = string->content;
  int num_to_shift = string->size - (to + 1);

  memmove(content + from, content + to + 1, num_to_shift);

  string->size -= (to - from + 1);
}
void BufferInsertCharAt(Buffer* buffer, i32 pos, char ch) {
  while (buffer->size + 1 >= buffer->capacity)
    DoubleCapacityIfFull(buffer);

  i32 textSize = buffer->size;
  char* text = buffer->content;

  memmove(text + pos + 1, text + pos, textSize - pos);

  text[pos] = ch;

  buffer->size++;
}

void BufferInsertChars(Buffer* buffer, char* chars, i32 len, i32 at) {
  while (buffer->size + len >= buffer->capacity)
    DoubleCapacityIfFull(buffer);

  buffer->size += len;

  char* from = buffer->content + at;
  char* to = buffer->content + at + len;
  memmove(to, from, buffer->size - at);

  for (i32 i = at; i < at + len; i++) {
    buffer->content[i] = chars[i - at];
  }
}

void InitChanges(Buffer* buffer, ChangeArena* arena) {
  Change* c = arena->contents;
  c->type = Added;
  c->groupId = 1;
  c->at = 4;
  c->size = 4;
  memmove(c->text, "bar\n", 4);

  c = NextChange(c);

  c->type = Added;
  c->groupId = 2;
  c->at = 4;
  c->size = 4;
  memmove(c->text, "ARM\n", 4);

  c = NextChange(c);

  c->type = Added;
  c->groupId = 3;
  c->at = 17;
  c->size = 6;
  memmove(c->text, "foo42\n", 6);

  c = NextChange(c);

  c->type = Removed;
  c->groupId = 3;
  c->at = 23;
  c->size = 3;
  memmove(c->text, "end", 3);

  arena->changesCount = 4;
  buffer->currentChangeGroup = 2;
}

Change* AddNewChange(Buffer* buffer) {
  Change* c = buffer->changeArena.contents;

  int targetGroup = buffer->currentChangeGroup;

  int changeCount = 0;
  while (changeCount < buffer->changeArena.changesCount &&    c->groupId <= targetGroup) {
    c = NextChange(c);
    changeCount++;
  }

  // TODO: here to decide if go next
  if (c->groupId == targetGroup && buffer->changeArena.changesCount != 0)
    c = NextChange(c);

  buffer->changeArena.changesCount = changeCount + 1;
  return c;
}

i32 shouldIncrementChangeGroups = 1;
void RemoveChars(Buffer* buffer, int from, int to) {
  if (shouldIncrementChangeGroups)
    buffer->currentChangeGroup++;

  Change* c = AddNewChange(buffer);

  int groupId = buffer->currentChangeGroup;

  c->type = Removed;
  c->groupId = groupId;
  c->at = from;
  c->size = to - from + 1;
  memmove(c->text, buffer->content + from, c->size);

  BufferRemoveChars(buffer, from, to);
}

void RemoveChar(Buffer* buffer, i32 at) {
  RemoveChars(buffer, at, at);
}

void InsertChars(Buffer* buffer, char* chars, i32 len, i32 at) {
  if (shouldIncrementChangeGroups)
    buffer->currentChangeGroup++;

  Change* c = AddNewChange(buffer);

  int groupId = buffer->currentChangeGroup;

  c->type = Added;
  c->groupId = groupId;
  c->at = at;
  c->size = len;
  memmove(c->text, chars, c->size);

  BufferInsertChars(buffer, chars, len, at);
}

void InsertCharAt(Buffer* buffer, i32 pos, char ch) {
  if (shouldIncrementChangeGroups)
    buffer->currentChangeGroup++;

  Change* c = AddNewChange(buffer);

  int groupId = buffer->currentChangeGroup;

  c->type = Added;
  c->groupId = groupId;
  c->at = pos;
  c->size = 1;
  c->text[0] = ch;

  BufferInsertCharAt(buffer, pos, ch);
}

// TODO: this is questionable, but should work for now. Usecase: formatting document
void ReplaceBufferContent(Buffer* buffer, char* newContent) {
  RemoveChars(buffer, 0, buffer->size - 1);
  shouldIncrementChangeGroups = 0;
  InsertChars(buffer, newContent, strlen(newContent), 0);
  shouldIncrementChangeGroups = 1;
  Change* first = buffer->changeArena.contents;
  Change* second = NextChange(first);
  int a = 0;
}

Buffer ReadFileIntoDoubledSizedBuffer(char* path) {
  u32 fileSize = GetMyFileSize(path);
  char* file = VirtualAllocateMemory(fileSize);
  ReadFileInto(path, fileSize, file);

  char* res = VirtualAllocateMemory(fileSize * 2);
  int fileSizeAfter = 0;
  for (i32 i = 0; i < fileSize; i++) {
    if (file[i] != '\r')
      res[fileSizeAfter++] = file[i];
  }
  VirtualFreeMemory(file);
  Buffer resFile = {.capacity = fileSize * 2, .size = fileSizeAfter, .content = res, .isSaved = 1};

  strcpy_s(resFile.filePath, 512, path);

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
i32 JumpWordWithPunctuationForward(Buffer* buffer) {
  char* text = buffer->content;
  i32 size = buffer->size;
  i32 pos = buffer->cursor;
  while (pos < size && !IsWhitespace(text[pos]))
    pos++;

  while (pos < size && IsWhitespace(text[pos]))
    pos++;

  return pos;
}

i32 JumpWordWithPunctuationBackward(Buffer* buffer) {
  char* text = buffer->content;
  i32 pos = MaxI32(buffer->cursor - 1, 0);
  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  while (pos > 0 && !IsWhitespace(text[pos - 1]))
    pos--;

  return pos;
}

i32 JumpWordBackward(Buffer* buffer) {
  char* text = buffer->content;
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
i32 JumpWordForward(Buffer* buffer) {
  char* text = buffer->content;
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
void InsertCharAtCursor(Buffer* buffer, char ch) {
  InsertCharAt(buffer, buffer->cursor, ch);
  buffer->cursor++;
}

Change* UndoChange(Buffer* buffer, ChangeArena* arena) {
  Change* first = buffer->changeArena.contents;
  Change* second = NextChange(first);
  Change* third = NextChange(second);
  Change* fourth = NextChange(third);

  Change* c = arena->contents;

  int targetGroup = buffer->currentChangeGroup;

  for (int i = 0; i < arena->changesCount && c->groupId != targetGroup; i++)
    c = NextChange(c);

  if (buffer->currentChangeGroup != 0 && c->groupId == buffer->currentChangeGroup) {
    buffer->currentChangeGroup -= 1;

    Change* changesToUndo[512];
    int changesCountToUndo = 0;

    while (c->groupId == targetGroup) {
      changesToUndo[changesCountToUndo++] = c;
      c = NextChange(c);
    }

    for (int i = changesCountToUndo - 1; i >= 0; i--) {
      Change* changeToUndo = changesToUndo[i];
      if (changeToUndo->type == Added)
        BufferRemoveChars(buffer, changeToUndo->at, changeToUndo->at + changeToUndo->size - 1);
      else
        BufferInsertChars(buffer, changeToUndo->text, changeToUndo->size, changeToUndo->at);
    }
    return changesToUndo[0];
  }
  return 0;
}

Change* RedoChange(Buffer* buffer, ChangeArena* arena) {
  Change* c = arena->contents;
  int targetGroup = buffer->currentChangeGroup + 1;
  int i = 0;
  while (i < arena->changesCount && c->groupId != targetGroup) {
    i++;
    c = NextChange(c);
  }

  if (c->groupId == targetGroup) {
    buffer->currentChangeGroup += 1;
    Change* lastChange = c;
    while (c->groupId == targetGroup) {
      // lastChange = c;
      if (c->type == Added)
        BufferInsertChars(buffer, c->text, c->size, c->at);
      else
        BufferRemoveChars(buffer, c->at, c->at + c->size - 1);
      c = NextChange(c);
    }
    return lastChange;
  }
  return 0;
}

i32 GetLineOffset(Buffer* buffer, i32 lineStart) {
  i32 l = lineStart;
  while (buffer->content[l] == ' ')
    l++;
  return l - lineStart;
}
i32 JumpParagraphDown(Buffer* buffer) {
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

i32 JumpParagraphUp(Buffer* buffer) {

  i32 res = buffer->cursor - 1;
  while (res > 0) {
    i32 lineStart = FindLineStart(buffer, res);
    i32 lineEnd = FindLineEnd(buffer, res);
    if (lineStart == lineEnd)
      return res;
    res = lineStart - 1;
  }
  return res;
}

// TODO: this could be optimized, no need to traverse whole file for each selection line
i32 GetLineLength(Buffer* text, i32 line) {
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
