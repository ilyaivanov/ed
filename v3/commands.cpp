#pragma once
#include "commandsBody.cpp"
#include "text.cpp"
#include "vim.cpp"

struct Key {
  int ctrl;
  int alt;
  u64 ch;
};

// Command = Operation? + Motion
Key currentCommand[255];
i32 currentCommandLen;

Key lastCommand[255];
i32 lastCommandLen;

Key lastMotion[255];
i32 lastMotionLen;

void FinishAndSaveCommand() {
  memmove(lastCommand, currentCommand, sizeof(Key) * ArrayLength(currentCommand));
  lastCommandLen = currentCommandLen;
  currentCommandLen = 0;
}

bool IsCh(char ch) {
  return currentCommandLen == 1 && currentCommand[0].ch == ch;
}

void HandleMotions(Key key, Buffer* buffer, Mode* mode, Window* window) {
  i32 nextCursor = buffer->cursorPos;
  bool isFinished = false;
  bool doNotUpdateDesiredOffset = false;

  if (IsCh('j')) {
    nextCursor = MoveDown(buffer);
    isFinished = true;
    doNotUpdateDesiredOffset = true;
  }
  if (IsCh('k')) {
    nextCursor = MoveUp(buffer);
    isFinished = true;
    doNotUpdateDesiredOffset = true;
  }
  if (IsCh('h')) {
    nextCursor = buffer->cursorPos - 1;
    isFinished = true;
  }
  if (IsCh('l')) {
    nextCursor = buffer->cursorPos + 1;
    isFinished = true;
  }

  if (nextCursor != buffer->cursorPos) {
    if (doNotUpdateDesiredOffset)
      UpdateCursorWithoutDesiredOffset(buffer, nextCursor);
    else
      UpdateCursor(buffer, nextCursor);
  }
  if (isFinished)
    currentCommandLen = 0;
}

void AppendKey(Key key, Buffer* buffer, Mode* mode, Window* window) {
  if (key.ch == 'q')
    Quit(window);

  currentCommand[currentCommandLen] = key;
  currentCommandLen++;
  HandleMotions(key, buffer, mode, window);
}
