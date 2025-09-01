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
} Item;

Item* AddChildAsText(Item* parent, const wchar_t* text, i32 at) {
  if (parent->childrenLen == parent->childrenCapacity) {
    parent->childrenCapacity = parent->childrenCapacity + 10;
    parent->children = (Item**)realloc(parent->children, parent->childrenCapacity * sizeof(Item*));
  }

  if (at < 0)
    at = parent->childrenLen;

  for (i32 i = parent->childrenLen; i > at; i--) {
    parent->children[i] = parent->children[i - 1];
  }

  Item* child = (Item*)calloc(1, sizeof(Item));
  parent->children[at] = child;
  parent->childrenLen++;
  child->textLen = wcslen(text);
  child->textCapacity = (child->textLen + 1) * sizeof(wchar_t);
  child->text = (wchar_t*)calloc(child->textCapacity, 1);
  memcpy(child->text, text, child->textLen * sizeof(wchar_t));
  child->parent = parent;

  return child;
}

Item* CreateItemLen(Item* parent, char* start, i32 len) {
  // memory leak
  c8* res = (c8*)calloc(len + 1, 2);
  for (i32 i = 0; i < len; i++) {
    res[i * 2] = start[i];
  }
  return AddChildAsText(parent, (wchar_t*)res, -1);
}

typedef struct StackEntry {
  int level;
  Item* item;
} StackEntry;

int IsSkipped(Item* item, i32 index) {
  return index > 0 && item->parent->children[index - 1]->isClosed && item->text[0] == L'}';
}

void AddChildrenToStack(StackEntry* stack, i32* stackLen, Item* parent, i32 level) {
  for (i32 i = parent->childrenLen - 1; i >= 0; i--) {
    Item* item = parent->children[i];
    if (!IsSkipped(item, i)) {
      stack[*stackLen] = (StackEntry){.level = level, .item = parent->children[i]};
      *stackLen += 1;
    }
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

Item* GetItemBelow(Item* item) {
  if (item->childrenLen > 0 && !item->isClosed)
    return item->children[0];
  else {
    int index = IndexOf(item);
    if (index < item->parent->childrenLen - 1)
      return item->parent->children[index + 1];
    else {
      Item* parent = item->parent;
      while (!IsRoot(parent) && IsLastItem(parent)) {
        parent = parent->parent;
      }
      if (!IsRoot(parent))
        return NextSibling(parent);
    }
  }
  return 0;
}

Item* GetItemAbove(Item* item) {
  int index = IndexOf(item);
  if (index > 0) {
    Item* prev = item->parent->children[index - 1];
    while (!prev->isClosed && prev->childrenLen > 0)
      prev = GetLastChild(prev);

    return prev;

  } else if (!IsRoot(item->parent)) {
    return item->parent;
  }

  return 0;
}

i32 GetVisibleChildCount(Item* item) {
  if (item->isClosed)
    return 0;

  i32 res = 0;

  for (i32 i = 0; i < item->childrenLen; i++) {
    Item* child = item->children[i];
    if (!IsSkipped(child, i))
      res++;
      
    res += GetVisibleChildCount(child);
  }

  return res;
}
