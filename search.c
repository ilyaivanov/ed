#pragma once

#include "vim.c"
#include "win32.c"
#include <string.h>

typedef struct EntryFound {
  u32 at;
  u32 len;
} EntryFound;

// TODO: use arena
EntryFound entriesAt[1024 * 10] = {0};
i32 currentEntry;
i32 isSearchVisible;
i32 entriesCount;

u8 searchTerm[255];
i32 searchLen;

inline char ToCharLower(char ch) {
  if (ch >= 'A' && ch <= 'Z')
    return ch + ('a' - 'A');

  return ch;
}

void FindEntries(Buffer* buffer) {
  i32 currentWordIndex = 0;
  entriesCount = 0;
  for (i32 i = 0; i < buffer->size; i++) {
    if (ToCharLower(buffer->content[i]) == ToCharLower(searchTerm[currentWordIndex])) {
      currentWordIndex++;
      if (currentWordIndex == searchLen) {
        entriesAt[entriesCount].at = i - searchLen + 1;
        entriesAt[entriesCount].len = searchLen;
        entriesCount++;
        currentWordIndex = 0;
      }
    } else {
      currentWordIndex = 0;
    }
  }

  currentEntry = 0;
}

void ClearEntries() {
  entriesCount = 0;
  memset(&searchTerm, 0, ArrayLength(searchTerm));
}

//
// Tags search
//
//
int isTagsSearchVisible = 0;
char tagsSearch[512];
int tagsSearchLen;
typedef struct CtagEntry {
  char name[100];
  char filename[50];
  char pattern[100];
  char type;
  char def[200];
} CtagEntry;
CtagEntry* entries;
int entiersCount;
void ReadCtagsFile() {
  if (!entries)
    entries = VirtualAllocateMemory(2000 * sizeof(CtagEntry));
  else
    memset(entries, 0, 2000 * sizeof(CtagEntry));

  tagsSearchLen = 0;
  entriesCount = 0;

  int fileSize = GetMyFileSize("tags");
  char* memory = VirtualAllocateMemory(fileSize);
  ReadFileInto("tags", fileSize, memory);

  int pos = 0;
  while (memory[pos] == '!') {
    while (memory[pos] != '\n')
      pos++;
    pos++;
  }
  // name  filename  pattern  type  info\n
  int currentField = 0;
  int fieldStart = pos;
  while (pos < fileSize) {
    while (memory[pos] != '\n') {

      if (memory[pos] == '\t') {
        char* destination;
        if (currentField == 0)
          destination = (char*)&entries[entriesCount].name;
        if (currentField == 1)
          destination = (char*)&entries[entriesCount].filename;
        if (currentField == 2)
          destination = (char*)&entries[entriesCount].pattern;
        if (currentField == 3)
          destination = (char*)&entries[entriesCount].type;

        if (currentField == 2) {
          char* start = &memory[fieldStart] + 2;
          int len = pos - fieldStart - 2 - 3;
          if (memory[pos - 4] == '$')
            len -= 1;
          memmove(destination, start, len);

        } else
          memmove(destination, &memory[fieldStart], pos - fieldStart);

        currentField++;
        fieldStart = pos + 1;
      }
      pos++;
    }

    memmove(&entries[entriesCount].def, &memory[fieldStart], pos - fieldStart);

    currentField = 0;
    entriesCount++;
    pos++;
    fieldStart = pos;
  }

  VirtualFreeMemory(memory);
}

int DoesEntryMatch(CtagEntry* entry) {
  int i = 0;
  while (i < tagsSearchLen) {
    char ch = entry->name[i];
    if (ToCharLower(ch) != ToCharLower(tagsSearch[i]))
      return 0;
    i++;
  }
  return 1;
}

CtagEntry* found[20] = {0};
void DrawTagsSearch(FontData* font) {
  int width = 500;
  int height = 800;
  int startX = canvas.width / 2 - width / 2;
  int x = startX;
  int y = 100;
  u32 bgColor = 0x333333;
  PaintRect(x, y, width, height, bgColor);

  Rect rect = {0, 0, canvas.width, canvas.height};
  for (int i = 0; i < tagsSearchLen; i++) {
    char ch = tagsSearch[i];
    CopyMonochromeTextureRectTo(&canvas, &rect, &font->textures[ch], x, y, 0xaaaaaa);
    x += font->charWidth;
  }

  y += font->charHeight;
  x = startX;

  int currentEntry = 0;
  if (tagsSearchLen >= 1) {
    for (int i = 0; i < entriesCount; i++) {
      if (DoesEntryMatch(&entries[i])) {
        found[currentEntry++] = &entries[i];
        if (currentEntry >= 10)
          break;
      }
    }
  }

  for (int i = 0; i < currentEntry; i++) {
    CtagEntry* a = found[i];
    char* t = a->name;
    while (*t) {
      CopyMonochromeTextureRectTo(&canvas, &rect, &font->textures[*t], x, y, 0xeeeeee);
      x += font->charWidth;
      t++;
    }
    y += font->charHeight;
    x = startX;
  }
}
