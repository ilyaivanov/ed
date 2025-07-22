#pragma once
#include "win32.cpp"

// this abstract away history of text changes

enum ChangeType { Added, Removed }; // Replaced

struct Change {
  ChangeType type;
  int at;

  // shows how much text added in a change, comes first in text, can be zero
  int addedTextSize;

  // shows how much text removed in a change, comes second in text, can be zero
  int removedTextSize;

  // two fields are non-zero for Replaced ChangeType
  char text[];
};

struct ChangeArena {
  Change* contents;
  int lastChangeIndex; // -1 is the default value
  int capacity;
  int changesCount;
};

struct Buffer {
  const char* path;
  char* file;
  i32 size;
  i32 capacity;
  ChangeArena changes;

  bool isSaved;
  i32 cursorPos;
  i32 desiredOffset;
  i32 selectionStart;
};

i32 ChangeSize(Change* c) {
  return sizeof(Change) + c->addedTextSize + c->removedTextSize;
}

Change* NextChange(Change* c) {
  return (Change*)(((u8*)c) + ChangeSize(c));
}

Change* GetChangeAt(ChangeArena* changes, int at) {
  Change* c = changes->contents;

  for (int i = 0; i < at; i++)
    c = NextChange(c);

  return c;
}

void DoubleCapacityIfFull(Buffer* buffer) {
  char* currentStr = buffer->file;
  buffer->capacity = (buffer->capacity == 0) ? 4 : (buffer->capacity * 2);
  buffer->file = (char*)VirtualAllocateMemory(buffer->capacity);
  if (currentStr) {
    memmove(buffer->file, currentStr, buffer->size);
    VirtualFreeMemory(currentStr);
  }
}

void DoubleChangeArenaCapacity(ChangeArena* changes) {
  i32 capacity = changes->capacity * 2;
  Change* newContents = (Change*)VirtualAllocateMemory(capacity);
  memmove(newContents, changes->contents, changes->capacity);
  VirtualFreeMemory(changes->contents);
  changes->contents = newContents;
  changes->capacity = capacity;
}

int GetChangeArenaSize(ChangeArena* arena) {
  Change* c = arena->contents;
  int res = 0;
  for (int i = 0; i < arena->changesCount; i++) {
    res += ChangeSize(c);
    c = NextChange(c);
  }
  return res;
}

Change* AddNewChange(Buffer* buffer, int changeSize) {
  int size = GetChangeArenaSize(&buffer->changes) + changeSize;
  if (size >= buffer->changes.capacity)
    DoubleChangeArenaCapacity(&buffer->changes);

  buffer->changes.lastChangeIndex += 1;
  buffer->changes.changesCount = buffer->changes.lastChangeIndex + 1;

  return GetChangeAt(&buffer->changes, buffer->changes.lastChangeIndex);
}

void BufferRemoveChars(Buffer* buffer, int from, int to) {
  char* content = buffer->file;
  int num_to_shift = buffer->size - (to + 1);

  memmove(content + from, content + to + 1, num_to_shift);

  buffer->size -= (to - from + 1);
}

void BufferInsertChars(Buffer* buffer, char* chars, i32 len, i32 at) {
  while (buffer->size + len >= buffer->capacity)
    DoubleCapacityIfFull(buffer);

  if (buffer->size != 0) {

    buffer->size += len;

    char* from = buffer->file + at;
    char* to = buffer->file + at + len;
    memmove(to, from, buffer->size - at);
  } else {
    buffer->size = len;
  }

  for (i32 i = at; i < at + len; i++) {
    buffer->file[i] = chars[i - at];
  }
}

void ApplyChange(Buffer* buffer, Change* c) {
  if (c->type == Added)
    BufferInsertChars(buffer, c->text, c->addedTextSize, c->at);
  if (c->type == Removed)
    BufferRemoveChars(buffer, c->at, c->at + c->addedTextSize - 1);
  // else if (c->type == Replaced) {
  //   BufferRemoveChars(buffer, c->at, c->at + c->size - 1);
  //   BufferInsertChars(buffer, c->text + c->size, c->newTextSize, c->at);
  // }
  buffer->isSaved = false;
}

void UndoChange(Buffer* buffer, Change* c) {
  if (c->type == Added)
    BufferRemoveChars(buffer, c->at, c->at + c->addedTextSize - 1);
  if (c->type == Removed)
    BufferInsertChars(buffer, c->text, c->addedTextSize, c->at);
  // else if (c->type == Replaced) {
  //   BufferRemoveChars(buffer, c->at, c->newTextSize - 1);
  //   BufferInsertChars(buffer, c->text, c->size, c->at);
  // }
  buffer->isSaved = false;
}

//
// API methods below
//
Change* UndoLastChange(Buffer* buffer) {
  int lastChangeAt = buffer->changes.lastChangeIndex;
  if (lastChangeAt >= 0) {
    Change* changeToUndo = GetChangeAt(&buffer->changes, lastChangeAt);
    buffer->changes.lastChangeIndex--;

    UndoChange(buffer, changeToUndo);
    return changeToUndo;
  }
  return 0;
}

Change* RedoLastChange(Buffer* buffer) {
  int pendingChangeAt = buffer->changes.lastChangeIndex + 1;
  if (pendingChangeAt < buffer->changes.changesCount) {
    Change* c = GetChangeAt(&buffer->changes, pendingChangeAt);

    ApplyChange(buffer, c);
    buffer->changes.lastChangeIndex++;
    return c;
  }
  return 0;
}

void RemoveChars(Buffer* buffer, int from, int to) {
  int changeSize = sizeof(Change) + (to - from + 1);
  Change* c = AddNewChange(buffer, changeSize);

  c->type = Removed;
  c->at = from;
  c->addedTextSize = to - from + 1;
  c->removedTextSize = 0;
  memmove(c->text, buffer->file + from, c->addedTextSize);

  ApplyChange(buffer, c);
}

void InsertChars(Buffer* buffer, char* chars, i32 len, i32 at) {
  int changeSize = sizeof(Change) + len;
  Change* c = AddNewChange(buffer, changeSize);

  c->type = Added;
  c->at = at;
  c->addedTextSize = len;
  c->removedTextSize = 0;
  memmove(c->text, chars, c->addedTextSize);

  ApplyChange(buffer, c);
}

// Change* ReplaceBufferContent(Buffer* buffer, char* newContent, i32 newContentLen) {
//   int changeSize = sizeof(Change) + buffer->size - 1 + newContentLen;

//   Change* c = AddNewChange(buffer, changeSize);

//   c->type = Replaced;
//   c->at = 0;
//   c->addedTextSize = buffer->size;
//   c->removedTextSize = newContentLen;
//   memmove(c->text, buffer->file, c->addedTextSize);
//   memmove(c->text + c->addedTextSize, newContent, c->removedTextSize);

//   ApplyChange(buffer, c);
//   return c;
// }
