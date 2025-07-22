#pragma once
#include "text.cpp"
#include "vim.cpp"
#include "win32.cpp"
#include <windows.h>

void Quit(Window* window) {
  PostQuitMessage(0);
  window->isTerminated = 1;
}

void UpdateCursorWithoutDesiredOffset(Buffer* buffer, i32 pos) {
  buffer->cursorPos = MaxI32(MinI32(pos, buffer->size - 1), 0);
}

void UpdateCursor(Buffer* buffer, i32 pos) {
  UpdateCursorWithoutDesiredOffset(buffer, pos);
  buffer->desiredOffset = buffer->cursorPos - FindLineStart(buffer, buffer->cursorPos);
}

i32 ApplyDesiredOffset(Buffer* buffer, i32 pos) {
  i32 lineEnd = FindLineEnd(buffer, pos);

  return MinI32(pos + buffer->desiredOffset, lineEnd);
}

i32 MoveDown(Buffer* buffer) {
  i32 end = FindLineEnd(buffer, buffer->cursorPos);
  return ApplyDesiredOffset(buffer, end + 1);
}

i32 MoveUp(Buffer* buffer) {
  i32 prevLineStart = FindLineStart(buffer, FindLineStart(buffer, buffer->cursorPos) - 1);
  if (prevLineStart < 0)
    prevLineStart = 0;

  return ApplyDesiredOffset(buffer, prevLineStart);
}

enum Mode { Normal, Insert, Visual, VisualLine };

struct SelectionRange {
  i32 start;
  i32 end;
};

SelectionRange GetSelectionRange(Buffer* buffer, Mode mode) {
  i32 selectionLeft = MinI32(buffer->selectionStart, buffer->cursorPos);
  i32 selectionRight = MaxI32(buffer->selectionStart, buffer->cursorPos);

  if (mode == Visual) {
    return SelectionRange{.start = selectionLeft, .end = selectionRight};
  } else if (mode == VisualLine) {
    selectionLeft = FindLineStart(buffer, selectionLeft);
    selectionRight = FindLineEnd(buffer, selectionRight);

    return SelectionRange{.start = selectionLeft, .end = selectionRight};
  }
  return {};
}

void RemoveSelection(Buffer* buffer, Mode mode) {
  SelectionRange range = GetSelectionRange(buffer, mode);

  RemoveChars(buffer, range.start, range.end);
  UpdateCursor(buffer, range.start);
}

void InsertCharAt(Buffer* buffer, char ch, i32 pos) {
  InsertChars(buffer, &ch, 1, pos);
}

i32 SkipWhitespace(Buffer* buffer, i32 pos) {
  while (pos < buffer->size && buffer->file[pos] == ' ')
    pos++;
  return pos;
}
i32 AddLineAbove(Buffer* buffer) {
  i32 offset = GetOffsetForLineAt(buffer, buffer->cursorPos);
  i32 start = FindLineStart(buffer, buffer->cursorPos);
  InsertCharAt(buffer, '\n', start);

  for (i32 i = 0; i < offset; i++)
    InsertCharAt(buffer, ' ', start + i);
  return start + offset;
}

// void AddLineBelow() {
//   i32 offset = GetOffsetForLineAt(buffer, selectedBuffer->cursorPos);
//   i32 end = FindLineEnd(selectedBuffer, selectedBuffer->cursorPos);
//   InsertCharAt('\n', end);

//   for (i32 i = 0; i < offset; i++)
//     InsertCharAt(' ', end + i + 1);
//   UpdateCursor(end + offset + 1);
//   mode = Insert;
//   ignoreNextCharEvent = 1;
// }
