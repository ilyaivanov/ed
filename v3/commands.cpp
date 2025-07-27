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
//
Key currentCommand[255];
i32 motionStartAt;
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
bool FinishIfMatch(const char* ch) {
  i32 len = strlen(ch);

  bool res = true;
  if ((currentCommandLen - motionStartAt) == len) {
    for (i32 i = 0; i < len; i++) {
      if (currentCommand[i + motionStartAt].ch != ch[i]) {
        res = false;
        break;
      }
    }
  } else {
    res = false;
  }

  if (res)
    isFinished = true;
  return res;
}

bool FinishIfLastKey(u64 key) {
  if (currentCommandLen > 0 && currentCommand[currentCommandLen - 1].ch == key) {
    isFinished = true;
    return true;
  }
  return false;
}

bool FinishIfSearchMatch(char searchCommand) {
  bool res = (currentCommandLen - motionStartAt) == 2 &&
             currentCommand[0 + motionStartAt].ch == searchCommand;
  if (res)
    isFinished = true;
  return res;
}

struct MotionResult {
  i32 cursor;
  bool shouldUpdateDesiredOffset;
  bool isLinewise;
  i32 left;
  i32 right;
};

bool HasOperation(Buffer* buffer, MotionResult* res) {
  i32 nextCursor = -1;

  bool doNotUpdateDesiredOffset = false;
  bool isLinewise = false;

  if (FinishIfMatch("j")) {
    nextCursor = MoveDown(buffer);
    doNotUpdateDesiredOffset = true;
    isLinewise = true;
  }
  if (FinishIfMatch("k")) {
    nextCursor = MoveUp(buffer);
    doNotUpdateDesiredOffset = true;
    isLinewise = true;
  }
  if (FinishIfMatch("gg")) {
    nextCursor = ApplyDesiredOffset(buffer, 0);
    doNotUpdateDesiredOffset = true;
    isLinewise = true;
  }
  if (FinishIfMatch("G")) {
    nextCursor = ApplyDesiredOffset(buffer, FindLineStart(buffer, buffer->size - 1));
    doNotUpdateDesiredOffset = true;
    isLinewise = true;
  }
  if (FinishIfMatch("0"))
    nextCursor = FindLineStart(buffer, buffer->cursorPos);

  if (FinishIfMatch("$") || FinishIfMatch("gl"))
    nextCursor = FindLineEnd(buffer, buffer->cursorPos);

  if (FinishIfMatch("^") || FinishIfMatch("gh"))
    nextCursor = SkipWhitespace(buffer, FindLineStart(buffer, buffer->cursorPos));

  if (FinishIfSearchMatch('f'))
    nextCursor =
        FindIndexOfCharForward(buffer, buffer->cursorPos + 1, currentCommand[1 + motionStartAt].ch);

  if (FinishIfSearchMatch('F'))
    nextCursor = FindIndexOfCharBackward(buffer, buffer->cursorPos - 1,
                                         currentCommand[1 + motionStartAt].ch);

  if (FinishIfSearchMatch('t')) {
    nextCursor =
        FindIndexOfCharForward(buffer, buffer->cursorPos + 1, currentCommand[1 + motionStartAt].ch);
    if (nextCursor != -1)
      nextCursor--;
  }
  if (FinishIfSearchMatch('T')) {
    nextCursor = FindIndexOfCharBackward(buffer, buffer->cursorPos - 1,
                                         currentCommand[1 + motionStartAt].ch);
    if (nextCursor != -1)
      nextCursor++;
  }
  if (FinishIfMatch("h"))
    nextCursor = buffer->cursorPos - 1;

  if (FinishIfMatch("l"))
    nextCursor = buffer->cursorPos + 1;

  if (FinishIfMatch("w"))
    nextCursor = JumpWordForward(buffer);
  if (FinishIfMatch("e"))
    nextCursor = JumpToTheEndOfWord(buffer);
  if (FinishIfMatch("E"))
    nextCursor = JumpToTheEndOfWordIgnorePunctuation(buffer);

  if (FinishIfMatch("b"))
    nextCursor = JumpWordBackward(buffer);

  if (FinishIfMatch("W"))
    nextCursor = JumpWordForwardIgnorePunctuation(buffer);

  if (FinishIfMatch("B"))
    nextCursor = JumpWordBackwardIgnorePunctuation(buffer);

  res->cursor = nextCursor;
  res->shouldUpdateDesiredOffset = !doNotUpdateDesiredOffset;
  res->isLinewise = isLinewise;
  return nextCursor != -1;
}

// Operation = Action + Motion
bool IsValidOperationWithMotion(const char* action, Buffer* buffer, MotionResult* motion) {
  i32 len = strlen(action);

  if (currentCommandLen > len) {
    for (i32 i = 0; i < len; i++) {
      if (currentCommand[i].ch != action[i])
        return false;
    }
    motionStartAt = len;

    if (HasOperation(buffer, motion)) {
      if (motion->cursor < buffer->cursorPos) {
        motion->left = MinI32(motion->cursor, buffer->cursorPos);
        motion->right = MaxI32(motion->cursor, buffer->cursorPos) - 1;
      } else {
        motion->left = MinI32(motion->cursor - 1, buffer->cursorPos);
        motion->right = MaxI32(motion->cursor - 1, buffer->cursorPos);
      }
      if (motion->isLinewise) {
        motion->left = FindLineStart(buffer, motion->left);
        motion->right = FindLineEnd(buffer, motion->right);
      }
      return true;
    }
  }

  return false;
}

void Run();
void AppendKey(Key key, Buffer* buffer, Mode* mode, Window* window) {
  if (key.ch == VK_ESCAPE) {
    if (*mode == Insert) {
      *mode = Normal;
    }
    currentCommandLen = 0;
  } else {

    currentCommand[currentCommandLen] = key;
    currentCommandLen++;

    if (*mode == Insert) {
      if (IsPrintable(key.ch)) {
        char ch = key.ch;
        if (ch == 13)
          ch = '\n';
        InsertCharAt(buffer, ch, buffer->cursorPos);

        buffer->cursorPos++;
      }
    } else if (*mode == Normal) {

      if (FinishIfMatch("i")) {
        *mode = Insert;
        isFinished = false;
      }
      if (FinishIfMatch("A")) {
        *mode = Insert;
        UpdateCursor(buffer, FindLineEnd(buffer, buffer->cursorPos) + 1);
        isFinished = false;
      }
      if (FinishIfMatch("C")) {
        isFinished = false;
        *mode = Insert;
        RemoveChars(buffer, buffer->cursorPos, FindLineEnd(buffer, buffer->cursorPos) - 1);
      }
      if (FinishIfMatch("D")) {
        RemoveChars(buffer, buffer->cursorPos, FindLineEnd(buffer, buffer->cursorPos) - 1);
      }

      if (currentCommandLen == 1 && currentCommand[0].ch == 'S' && currentCommand[0].ctrl) {
        SaveBuffer(buffer);
        isFinished = true;
      }
      
      if (currentCommandLen == 1 && currentCommand[0].ch == 'r' && currentCommand[0].alt) {
        Run();
        isFinished = true;
      }

      if (FinishIfMatch("q"))
        Quit(window);
      if (FinishIfLastKey(VK_F11)) {
        window->isFullscreen = !window->isFullscreen;
        SetFullscreen(window->windowHandle, window->isFullscreen);
      }

      MotionResult res = {0};
      if (IsValidOperationWithMotion("d", buffer, &res)) {

        RemoveChars(buffer, res.left, res.right);
        if (res.isLinewise)
          UpdateCursor(buffer, ApplyDesiredOffset(buffer, FindLineStart(buffer, res.left)));
        else
          UpdateCursor(buffer, res.left);
      } else if (IsValidOperationWithMotion("c", buffer, &res)) {
        isFinished = false;
        RemoveChars(buffer, res.left, res.right);
        if (res.isLinewise)
          UpdateCursor(buffer, ApplyDesiredOffset(buffer, FindLineStart(buffer, res.left)));
        else
          UpdateCursor(buffer, res.left);

        *mode = Insert;
      } else if (IsValidOperationWithMotion("gU", buffer, &res)) {

        for (i32 i = res.left; i <= res.right; i++)
          buffer->file[i] = ToUppercase(buffer->file[i]);
      } else if (IsValidOperationWithMotion("gu", buffer, &res)) {

        for (i32 i = res.left; i <= res.right; i++)
          buffer->file[i] = ToLowercase(buffer->file[i]);
      } else {
        if (HasOperation(buffer, &res)) {
          if (res.cursor != buffer->cursorPos) {
            if (res.shouldUpdateDesiredOffset)
              UpdateCursor(buffer, res.cursor);
            else
              UpdateCursorWithoutDesiredOffset(buffer, res.cursor);
          }
        }
      }
    }

    motionStartAt = 0;

    if (isFinished) {
      currentCommandLen = 0;
      isFinished = false;
    }
  }
}
