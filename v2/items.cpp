#pragma once
#include "win32.cpp"
#include "windows.h"
#include <stdlib.h>

struct Item {
  const char* title;
  bool isTaken;
  bool isClosed;
  Item** children;
  Item* parent;
  i32 childrenCount;
  i32 childrenCapacity;

  Item* closingBracket;
};

Item allItems[10 * 1024];
Item* FindFreeItem() {
  for (i32 i = 0; i < ArrayLength(allItems); i++) {
    if (!allItems[i].isTaken)
      return &allItems[i];
  }
  return 0;
}

Item* AddChild(Item* parent, const char* title) {
  if (parent->childrenCapacity == parent->childrenCount) {
    i32 newCapacity = parent->childrenCapacity == 0 ? 2 : parent->childrenCapacity * 2;

    Item** temp = parent->children;
    parent->children = (Item**)calloc(newCapacity, sizeof(Item*));
    memmove(parent->children, temp, parent->childrenCapacity * sizeof(Item*));
    parent->childrenCapacity = newCapacity;
    free(temp);
  }

  Item* newItem = FindFreeItem();
  if (newItem) {
    newItem->isTaken = 1;
    newItem->title = title;
    newItem->parent = parent;
    parent->children[parent->childrenCount++] = newItem;
    return newItem;
  }
  return 0;
}

i32 GetItemIndex(Item* item) {
  Item* parent = item->parent;
  for (i32 i = 0; i < parent->childrenCount; i++) {
    if (parent->children[i] == item)
      return i;
  }
  return -1;
}

Item* GetItemPrevSibling(Item* item) {
  i32 index = GetItemIndex(item);
  if (index == 0)
    return 0;
  return item->parent->children[index - 1];
}

i32 IsRoot(Item* item) {
  return !item->parent;
}

i32 IsSkippedClosedBracket(Item* item) {
  if (item->title[0] == '}') {
    Item* prev = GetItemPrevSibling(item);
    if (prev && prev->isClosed)
      return 1;
  }
  return 0;
}

Item* GetItemBelow(Item* item) {
  if (item->childrenCount > 0 && !item->isClosed)
    return item->children[0];
  else {
    Item* parent = item->parent;
    i32 index = GetItemIndex(item);
    if (index != (parent->childrenCount - 1)) {
      return parent->children[index + 1];
    }

    while (!IsRoot(parent) && GetItemIndex(parent) == (parent->parent->childrenCount - 1)) {
      parent = parent->parent;
    }

    if (IsRoot(parent))
      return 0;
    return parent->parent->children[GetItemIndex(parent) + 1];
  }
}

Item* GetMostNestedItem(Item* item) {
  while (!item->isClosed && item->childrenCount > 0)
    item = item->children[item->childrenCount - 1];
  return item;
}
