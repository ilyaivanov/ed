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

bool isFinished = false;
bool FinishIfMatch(char ch) {
  bool res = currentCommandLen == 1 && currentCommand[0].ch == ch;
  if (res)
    isFinished = true;
  return res;
}

bool FinishIfSearchMatch(char searchCommand) {
  bool res = currentCommandLen == 2 && currentCommand[0].ch == searchCommand;
  if (res)
    isFinished = true;
  return res;
}

void HandleMotions(Key key, Buffer* buffer, Mode* mode, Window* window) {
  i32 nextCursor = -1;

  bool doNotUpdateDesiredOffset = false;

  if (FinishIfMatch('j')) {
    nextCursor = MoveDown(buffer);
    doNotUpdateDesiredOffset = true;
  }
  if (FinishIfMatch('k')) {
    nextCursor = MoveUp(buffer);
    doNotUpdateDesiredOffset = true;
  }
  if (FinishIfSearchMatch('f'))
    nextCursor = FindIndexOfCharForward(buffer, buffer->cursorPos + 1, currentCommand[1].ch);

  if (FinishIfSearchMatch('F'))
    nextCursor = FindIndexOfCharBackward(buffer, buffer->cursorPos - 1, currentCommand[1].ch);

  if (FinishIfSearchMatch('t')) {
    nextCursor = FindIndexOfCharForward(buffer, buffer->cursorPos + 1, currentCommand[1].ch);
    if (nextCursor != -1)
      nextCursor--;
  }
  if (FinishIfSearchMatch('T')) {
    nextCursor = FindIndexOfCharBackward(buffer, buffer->cursorPos - 1, currentCommand[1].ch);
    if (nextCursor != -1)
      nextCursor++;
  }
  if (FinishIfMatch('h'))
    nextCursor = buffer->cursorPos - 1;

  if (FinishIfMatch('l'))
    nextCursor = buffer->cursorPos + 1;

  if (FinishIfMatch('w'))
    nextCursor = JumpWordForward(buffer);
  if (FinishIfMatch('e'))
    nextCursor = JumpToTheEndOfWord(buffer);
  if (FinishIfMatch('E'))
    nextCursor = JumpToTheEndOfWordIgnorePunctuation(buffer);

  if (FinishIfMatch('b'))
    nextCursor = JumpWordBackward(buffer);

  if (FinishIfMatch('W'))
    nextCursor = JumpWordForwardIgnorePunctuation(buffer);

  if (FinishIfMatch('B'))
    nextCursor = JumpWordBackwardIgnorePunctuation(buffer);

  if (nextCursor >= 0 && nextCursor != buffer->cursorPos) {
    if (doNotUpdateDesiredOffset)
      UpdateCursorWithoutDesiredOffset(buffer, nextCursor);
    else
      UpdateCursor(buffer, nextCursor);
  }
  if (isFinished) {
    currentCommandLen = 0;
    isFinished = false;
  }
}

void AppendKey(Key key, Buffer* buffer, Mode* mode, Window* window) {
  if (key.ch == VK_ESCAPE)
    currentCommandLen = 0;
  else {
    currentCommand[currentCommandLen] = key;
    currentCommandLen++;

    if (FinishIfMatch('q'))
      Quit(window);

    HandleMotions(key, buffer, mode, window);
  }
}
