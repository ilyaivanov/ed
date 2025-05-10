#pragma once

#include "vim.c"
#include "win32.c"

typedef struct EntryFound
{
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

inline char ToCharLower(char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch + ('a' - 'A');

    return ch;
}

void FindEntries(Buffer *buffer)
{
    i32 currentWordIndex = 0;
    entriesCount = 0;
    for (i32 i = 0; i < buffer->size; i++)
    {
        if (ToCharLower(buffer->content[i]) ==
            ToCharLower(searchTerm[currentWordIndex]))
        {
            currentWordIndex++;
            if (currentWordIndex == searchLen)
            {
                entriesAt[entriesCount].at = i - searchLen + 1;
                entriesAt[entriesCount].len = searchLen;
                entriesCount++;
                currentWordIndex = 0;
            }
        }
        else
        {
            currentWordIndex = 0;
        }
    }

    currentEntry = 0;
}

void ClearEntries()
{
    entriesCount = 0;
    memset(&searchTerm, 0, ArrayLength(searchTerm));
}
