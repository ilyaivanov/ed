#pragma once
#include "win32.cpp"
#include <malloc.h>

typedef struct Item {
  wchar_t* text;
  i32 textLen;
  i32 textCapacity;
  i32 isClosed;

  struct Item** children;
  struct Item* parent;
  i32 childrenLen;
  i32 childrenCapacity;
  u32 fileAttrs;
  i32 isFileModified;
} Item;

void DoubleTextCapacity(Item* item) {
  if (item->textCapacity == 0)
    item->textCapacity = 8;
  else
    item->textCapacity = item->textCapacity * 2;

  item->text = (wchar_t*)realloc(item->text, item->textCapacity);
}

void AddChildAt(Item* parent, Item* child, i32 at) {

  if (parent->childrenLen == parent->childrenCapacity) {
    parent->childrenCapacity = parent->childrenCapacity + 10;
    parent->children = (Item**)realloc(parent->children, parent->childrenCapacity * sizeof(Item*));
  }

  if (at < 0)
    at = parent->childrenLen;

  for (i32 i = parent->childrenLen; i > at; i--) {
    parent->children[i] = parent->children[i - 1];
  }

  parent->children[at] = child;
  parent->childrenLen++;

  child->parent = parent;
}

Item* AddChildAsText(Item* parent, i32 at, const wchar_t* text, i32 len) {
  Item* child = (Item*)calloc(1, sizeof(Item));

  AddChildAt(parent, child, at);

  child->textLen = len;
  child->textCapacity = (child->textLen + 1) * sizeof(wchar_t);
  child->text = (wchar_t*)calloc(child->textCapacity, 1);
  memcpy(child->text, text, child->textLen * sizeof(wchar_t));

  return child;
}

typedef struct StackEntry {
  int level;
  Item* item;
} StackEntry;

void AddChildrenToStack(StackEntry* stack, i32* stackLen, Item* parent, i32 level) {
  for (i32 i = parent->childrenLen - 1; i >= 0; i--) {
    stack[*stackLen] = (StackEntry){.level = level, .item = parent->children[i]};
    *stackLen += 1;
  }
}

i32 IsRoot(Item* item) {
  return !item->parent;
}

i32 IndexOf(Item* item) {
  Item* parent = item->parent;
  for (int i = 0; i < parent->childrenLen; i++) {
    if (parent->children[i] == item)
      return i;
  }
  return -1;
}

Item* NextSibling(Item* item) {
  int index = IndexOf(item);
  return item->parent->children[index + 1];
}

Item* GetLastChild(Item* parent) {
  return parent->children[parent->childrenLen - 1];
}

i32 IsLastItem(Item* item) {
  return IndexOf(item) == (item->parent->childrenLen - 1);
}

int IsFolder(Item* item) {
  return item->fileAttrs & FILE_ATTRIBUTE_DIRECTORY;
}

i32 FillItemPath(c16* buff, Item* item, const c16* rootPath) {
  Item* path[20] = {0};
  i32 pathLen = 0;

  Item* current = item;

  while (!IsRoot(current)) {
    path[pathLen++] = current;
    current = current->parent;
  }

  i32 len = 0;
  len = Appendw(buff, len, rootPath);
  for (i32 i = pathLen - 1; i >= 0; i--) {
    len = Appendw(buff, len, L"\\");
    len = Appendw(buff, len, path[i]->text);
  }

  return len;
}

Item* GetItemBelow(Item* item) {
  Item* res = 0;

  if (item->childrenLen > 0 && !item->isClosed)
    res = item->children[0];
  else {
    int index = IndexOf(item);
    if (index < item->parent->childrenLen - 1)
      res = item->parent->children[index + 1];
    else {
      Item* parent = item->parent;
      while (!IsRoot(parent) && IsLastItem(parent)) {
        parent = parent->parent;
      }
      if (!IsRoot(parent))
        res = NextSibling(parent);
    }
  }

  return res;
}

Item* GetMostNestedItem(Item* item) {

  while (!item->isClosed && item->childrenLen > 0)
    item = GetLastChild(item);

  return item;
}

Item* GetItemAbove(Item* item) {
  int index = IndexOf(item);
  Item* res = 0;

  if (index > 0) {
    res = GetMostNestedItem(item->parent->children[index - 1]);
  } else if (!IsRoot(item->parent)) {
    res = item->parent;
  }

  return res;
}

Item* GetNextSibling(Item* item) {
  Item* parent = item->parent;
  i32 index = IndexOf(item);
  Item* res = 0;
  if (index < parent->childrenLen - 1)
    res = parent->children[index + 1];
  return res;
}

Item* GetPrevSibling(Item* item) {
  Item* parent = item->parent;
  i32 index = IndexOf(item);
  Item* res = 0;
  if (index > 0)
    res = parent->children[index - 1];
  return res;
}

i32 GetVisibleChildCount(Item* item) {
  if (item->isClosed)
    return 0;

  i32 res = 0;

  for (i32 i = 0; i < item->childrenLen; i++) {
    Item* child = item->children[i];
    res++;

    res += GetVisibleChildCount(child);
  }

  return res;
}

void InsertCharAtCursor(Item* item, i32 pos, wchar_t ch) {
  if (item->textLen * 2 == item->textCapacity)
    DoubleTextCapacity(item);

  for (i32 i = item->textLen; i > pos; i--) {
    item->text[i] = item->text[i - 1];
  }
  item->text[pos] = ch;
  item->textLen++;
}

void RemoveChars(Item* item, i32 from, i32 to) {
  int num_to_shift = (item->textLen - (to + 1)) * 2;

  memmove(item->text + from, item->text + to + 1, num_to_shift);

  item->textLen -= (to - from + 1);
}

i32 JumpWordForward(Item* item, i32 p) {
  wchar_t* text = item->text;
  i32 size = item->textLen;
  if (IsWhitespace(text[p])) {
    while (p < size && IsWhitespace(text[p]))
      p++;
  } else {
    if (IsAlphaNumeric(text[p])) {
      while (p < size && IsAlphaNumeric(text[p]))
        p++;
    } else {
      while (p < size && IsPunctuation(text[p]))
        p++;
    }
    while (p < size && IsWhitespace(text[p]))
      p++;
  }
  return p;
}

i32 JumpWordBackward(Item* item, i32 p) {
  wchar_t* text = item->text;

  p = Max(p - 1, 0);
  i32 isStartedAtWhitespace = IsWhitespace(text[p]);

  while (p > 0 && IsWhitespace(text[p]))
    p--;

  if (IsAlphaNumeric(text[p])) {
    while (p > 0 && IsAlphaNumeric(text[p]))
      p--;
  } else {
    while (p > 0 && IsPunctuation(text[p]))
      p--;
  }
  if (p != 0)
    p++;

  if (!isStartedAtWhitespace) {
    while (p > 0 && IsWhitespace(text[p]))
      p--;
  }

  return p;
}

void RemoveItemFromParent(Item* item) {
  Item* parent = item->parent;
  i32 index = IndexOf(item);
  for (i32 i = index; i < parent->childrenLen - 1; i++) {
    parent->children[i] = parent->children[i + 1];
  }
  parent->childrenLen--;
}

void FreeItemAndSubitems(Item* item) {

  for (i32 i = 0; i < item->childrenLen; i++) {
    FreeItemAndSubitems(item->children[i]);
  }
  free(item->children);
  free(item->text);
  free(item);
}

void RemoveItem(Item* item) {
  RemoveItemFromParent(item);
  FreeItemAndSubitems(item);
}

//
// Moving items around 
//
void SwapUp(Item* item) {
  i32 index = IndexOf(item);
  Item* parent = item->parent;
  if (index > 0) {
    Item* tmp = parent->children[index - 1];
    parent->children[index - 1] = item;
    parent->children[index] = tmp;
  }
}

void SwapLeft(Item* item) {
  if (!IsRoot(item->parent)) {
    RemoveItemFromParent(item);
    AddChildAt(item->parent->parent, item, IndexOf(item->parent) + 1);
  }
}

void SwapRight(Item* item) {
  i32 index = IndexOf(item);
  if (index > 0) {
    RemoveItemFromParent(item);
    Item* prev = item->parent->children[index - 1];
    AddChildAt(prev, item, -1);
    prev->isClosed = 0;
  }
}

void SwapDown(Item* item) {
  i32 index = IndexOf(item);
  Item* parent = item->parent;
  if (index < parent->childrenLen - 1) {
    Item* tmp = parent->children[index + 1];
    parent->children[index + 1] = item;
    parent->children[index] = tmp;
  }
}
