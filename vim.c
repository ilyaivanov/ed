#pragma once
#include <string.h>

typedef enum ChangeType { Added, Removed, Replaced } ChangeType;

typedef struct Change {
  ChangeType type;
  int at;
  int size;

  int newTextSize;
  char text[];
} Change;

typedef struct ChangeArena {
  Change* contents;
  int lastChangeIndex; // -1 is the default value
  int capacity;
  int changesCount;
} ChangeArena;

typedef struct Buffer {
  char* content;
  i32 size;
  i32 capacity;

  i32 cursor;
  i32 selectionStart;
  i32 oposingCharAt;
  i32 isSaved;
  char filePath[512];

  ChangeArena changeArena;
} Buffer;

typedef struct Key {
  int ctrl;
  int shift;
  int alt;
  char ch;
} Key;

Key lastCommand[KB(2 * 1024)];
int lastCommandLen;

void InitChangeArena(Buffer* buffer) {
  buffer->changeArena.contents = VirtualAllocateMemory(KB(40));
  buffer->changeArena.capacity = KB(40);
  buffer->changeArena.lastChangeIndex = -1;
}

void ChangeArenaDoubleMemory(ChangeArena* arena, int newSize) {
  Change* newContents = VirtualAllocateMemory(newSize);
  memmove(newContents, arena->contents, arena->capacity);
  VirtualFreeMemory(arena->contents);
  arena->contents = newContents;
  arena->capacity = newSize;
}

#define ChangeSize(c) (sizeof(Change) + c->size + c->newTextSize)
#define NextChange(c) ((Change*)(((u8*)c) + ChangeSize(c)))

int GetChangeArenaSize(ChangeArena* arena) {
  Change* c = arena->contents;
  int res = 0;
  for (int i = 0; i < arena->changesCount; i++) {
    res += ChangeSize(c);
    c = NextChange(c);
  }
  return res;
}

//
// String stuff
//
int StrContainsChar(char* str, char ch) {
  while (*str) {
    if (*str == ch)
      return 1;
    str++;
  }
  return *str == ch;
}

int IsStrEqual(char* s1, char* s2) {
  while (*s1 && *s2) {
    if (*s1 != *s2)
      return 0;
    s1++;
    s2++;
  }
  return *s1 == *s2;
}

int StrEndsWith(char* str, char* seq) {
  int len = strlen(str);
  int seqLen = strlen(seq);
  if (seqLen > len)
    return -1;
  char* end = str + len - seqLen;
  return IsStrEqual(end, seq);
}

inline char ToCharLower(char ch) {
  if (ch >= 'A' && ch <= 'Z')
    return ch + ('a' - 'A');

  return ch;
}

int StrIndexOfCaseInsensitive(char* s, char* sub) {
  if (!*sub)
    return 0; // Empty substring matches at index 0

  for (int i = 0; s[i]; ++i) {
    int j = 0;
    while (sub[j] && ToCharLower(s[i + j]) == ToCharLower(sub[j])) {
      ++j;
    }
    if (!sub[j]) {
      return i; // Match found
    }
  }

  return -1; // No match
}

i32 FindLineStart(Buffer* buffer, i32 pos) {
  while (pos > 0 && buffer->content[pos - 1] != '\n')
    pos--;

  return pos;
}

i32 FindNextChar(Buffer* buffer, i32 from, char ch) {
  while (from < buffer->size) {
    if (buffer->content[from] == ch)
      return from;
    from++;
  }
  return -1;
}

i32 FindLineEnd(Buffer* buffer, i32 pos) {
  i32 textSize = buffer->size;
  char* text = buffer->content;
  while (pos < textSize && text[pos] != '\n')
    pos++;

  return pos;
}

void DoubleCapacityIfFull(Buffer* buffer) {
  char* currentStr = buffer->content;
  buffer->capacity = (buffer->capacity == 0) ? 4 : (buffer->capacity * 2);
  buffer->content = VirtualAllocateMemory(buffer->capacity);
  if (currentStr) {
    memmove(buffer->content, currentStr, buffer->size);
    VirtualFreeMemory(currentStr);
  }
}

void BufferRemoveChars(Buffer* string, int from, int to) {
  char* content = string->content;
  int num_to_shift = string->size - (to + 1);

  memmove(content + from, content + to + 1, num_to_shift);

  string->size -= (to - from + 1);
}
void BufferInsertCharAt(Buffer* buffer, i32 pos, char ch) {
  while (buffer->size + 1 >= buffer->capacity)
    DoubleCapacityIfFull(buffer);

  i32 textSize = buffer->size;
  char* text = buffer->content;

  memmove(text + pos + 1, text + pos, textSize - pos);

  text[pos] = ch;

  buffer->size++;
}

void BufferInsertChars(Buffer* buffer, char* chars, i32 len, i32 at) {
  while (buffer->size + len >= buffer->capacity)
    DoubleCapacityIfFull(buffer);

  if (buffer->size != 0) {

    buffer->size += len;

    char* from = buffer->content + at;
    char* to = buffer->content + at + len;
    memmove(to, from, buffer->size - at);
  } else {
    buffer->size = len;
  }

  for (i32 i = at; i < at + len; i++) {
    buffer->content[i] = chars[i - at];
  }
}

Change* GetChangeAt(Buffer* buffer, int at) {
  Change* c = buffer->changeArena.contents;

  for (int i = 0; i < at; i++)
    c = NextChange(c);

  return c;
}

Change* AddNewChange(Buffer* buffer, int changeSize) {
  int size = GetChangeArenaSize(&buffer->changeArena) + changeSize;
  if (size >= buffer->changeArena.capacity)
    ChangeArenaDoubleMemory(&buffer->changeArena, size * 2);

  buffer->changeArena.lastChangeIndex += 1;
  buffer->changeArena.changesCount = buffer->changeArena.lastChangeIndex + 1;

  return GetChangeAt(buffer, buffer->changeArena.lastChangeIndex);
}

void ApplyChange(Buffer* buffer, Change* c) {
  if (c->type == Added)
    BufferInsertChars(buffer, c->text, c->size, c->at);
  else if (c->type == Removed)
    BufferRemoveChars(buffer, c->at, c->at + c->size - 1);
  else if (c->type == Replaced) {
    BufferRemoveChars(buffer, c->at, c->at + c->size - 1);
    BufferInsertChars(buffer, c->text + c->size, c->newTextSize, c->at);
  }
  buffer->isSaved = 0;
}

void UndoChange(Buffer* buffer, Change* c) {
  if (c->type == Added)
    BufferRemoveChars(buffer, c->at, c->at + c->size - 1);
  else if (c->type == Removed)
    BufferInsertChars(buffer, c->text, c->size, c->at);
  else if (c->type == Replaced) {
    BufferRemoveChars(buffer, c->at, c->newTextSize - 1);
    BufferInsertChars(buffer, c->text, c->size, c->at);
  }
  buffer->isSaved = 0;
}

void RemoveChars(Buffer* buffer, int from, int to) {
  int changeSize = sizeof(Change) + (to - from + 1);
  Change* c = AddNewChange(buffer, changeSize);

  c->type = Removed;
  c->at = from;
  c->size = to - from + 1;
  c->newTextSize = 0;
  memmove(c->text, buffer->content + from, c->size);

  ApplyChange(buffer, c);
}

void RemoveChar(Buffer* buffer, i32 at) {
  RemoveChars(buffer, at, at);
}

void InsertChars(Buffer* buffer, char* chars, i32 len, i32 at) {
  int changeSize = sizeof(Change) + len;
  Change* c = AddNewChange(buffer, changeSize);

  c->type = Added;
  c->at = at;
  c->size = len;
  c->newTextSize = 0;
  memmove(c->text, chars, c->size);

  ApplyChange(buffer, c);
}

void InsertCharAt(Buffer* buffer, i32 pos, char ch) {
  int changeSize = sizeof(Change) + 1;
  Change* c = AddNewChange(buffer, changeSize);

  c->type = Added;
  c->at = pos;
  c->size = 1;
  c->newTextSize = 0;
  c->text[0] = ch;

  ApplyChange(buffer, c);
}

void ReplaceBufferContent(Buffer* buffer, char* newContent) {
  int newContentLen = strlen(newContent);
  int changeSize = sizeof(Change) + buffer->size - 1 + newContentLen;

  Change* c = AddNewChange(buffer, changeSize);

  c->type = Replaced;
  c->at = 0;
  c->size = buffer->size;
  c->newTextSize = newContentLen;
  memmove(c->text, buffer->content, c->size);
  memmove(c->text + c->size, newContent, c->newTextSize);

  ApplyChange(buffer, c);
}

void CopyStrIntoBuffer(Buffer* buffer, char* str, int size) {
  int fileSizeAfter = 0;
  for (i32 i = 0; i < size; i++) {
    if (str[i] != '\r')
      buffer->content[fileSizeAfter++] = str[i];
  }
  buffer->size = fileSizeAfter;
}

Buffer ReadFileIntoDoubledSizedBuffer(char* path) {
  u32 fileSize = GetMyFileSize(path);
  char* file = VirtualAllocateMemory(fileSize);
  ReadFileInto(path, fileSize, file);

  char* res = VirtualAllocateMemory(fileSize * 2);
  int fileSizeAfter = 0;
  for (i32 i = 0; i < fileSize; i++) {
    if (file[i] != '\r')
      res[fileSizeAfter++] = file[i];
  }
  VirtualFreeMemory(file);
  Buffer resFile = {.capacity = fileSize * 2, .size = fileSizeAfter, .content = res, .isSaved = 1 , .oposingCharAt = -1};

  strcpy_s(resFile.filePath, 512, path);

  return resFile;
}
u32 IsWhitespace(char ch) {
  return ch == ' ' || ch == '\n';
}
u32 IsAlphaNumeric(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}
u32 IsPunctuation(char ch) {
  // use a lookup table
  const char* punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

  const char* p = punctuation;
  while (*p) {
    if (ch == *p) {
      return 1;
    }
    p++;
  }
  return 0;
}
i32 JumpWordWithPunctuationForward(Buffer* buffer) {
  char* text = buffer->content;
  i32 size = buffer->size;
  i32 pos = buffer->cursor;
  while (pos < size && !IsWhitespace(text[pos]))
    pos++;

  while (pos < size && IsWhitespace(text[pos]))
    pos++;

  return pos;
}

i32 JumpWordWithPunctuationBackward(Buffer* buffer) {
  char* text = buffer->content;
  i32 pos = MaxI32(buffer->cursor - 1, 0);
  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  while (pos > 0 && !IsWhitespace(text[pos - 1]))
    pos--;

  return pos;
}

i32 JumpWordBackward(Buffer* buffer) {
  char* text = buffer->content;
  i32 pos = MaxI32(buffer->cursor - 1, 0);
  i32 size = buffer->size;
  i32 isStartedAtWhitespace = IsWhitespace(text[pos]);

  while (pos > 0 && IsWhitespace(text[pos]))
    pos--;

  if (IsAlphaNumeric(text[pos])) {
    while (pos > 0 && IsAlphaNumeric(text[pos]))
      pos--;
  } else {
    while (pos > 0 && IsPunctuation(text[pos]))
      pos--;
  }
  pos++;

  if (!isStartedAtWhitespace) {
    while (pos > 0 && IsWhitespace(text[pos]))
      pos--;
  }

  return pos;
}
i32 JumpWordForward(Buffer* buffer) {
  char* text = buffer->content;
  i32 pos = buffer->cursor;
  i32 size = buffer->size;
  if (IsWhitespace(text[pos])) {
    while (pos < size && IsWhitespace(text[pos]))
      pos++;
  } else {
    if (IsAlphaNumeric(text[pos])) {
      while (pos < size && IsAlphaNumeric(text[pos]))
        pos++;
    } else {
      while (pos < size && IsPunctuation(text[pos]))
        pos++;
    }
    while (pos < size && IsWhitespace(text[pos]))
      pos++;
  }

  return pos;
}

void InsertCharAtCursor(Buffer* buffer, char ch) {
  InsertCharAt(buffer, buffer->cursor, ch);
  buffer->cursor++;
}

Change* UndoLastChange(Buffer* buffer) {
  int lastChangeAt = buffer->changeArena.lastChangeIndex;
  if (lastChangeAt >= 0) {
    Change* changeToUndo = GetChangeAt(buffer, lastChangeAt);
    buffer->changeArena.lastChangeIndex--;

    UndoChange(buffer, changeToUndo);
    return changeToUndo;
  }
  return 0;
}

Change* RedoLastChange(Buffer* buffer) {
  int pendingChangeAt = buffer->changeArena.lastChangeIndex + 1;
  if (pendingChangeAt < buffer->changeArena.changesCount) {
    Change* c = GetChangeAt(buffer, pendingChangeAt);

    ApplyChange(buffer, c);
    buffer->changeArena.lastChangeIndex++;
    return c;
  }
  return 0;
}

i32 GetLineOffset(Buffer* buffer, i32 lineStart) {
  i32 l = lineStart;
  while (buffer->content[l] == ' ')
    l++;
  return l - lineStart;
}

i32 JumpParagraphDown(Buffer* buffer) {
  i32 res = buffer->cursor + 1;
  while (res <= buffer->size) {
    i32 lineStart = FindLineStart(buffer, res);
    i32 lineEnd = FindLineEnd(buffer, res);
    if (lineStart == lineEnd)
      return res;
    res = lineEnd + 1;
  }
  return res;
}

i32 JumpParagraphUp(Buffer* buffer) {
  i32 res = buffer->cursor - 1;
  while (res > 0) {
    i32 lineStart = FindLineStart(buffer, res);
    i32 lineEnd = FindLineEnd(buffer, res);
    if (lineStart == lineEnd)
      return res;
    res = lineStart - 1;
  }
  return res;
}

void BreakLineAtCursor(Buffer* buffer) {
  int offset = GetLineOffset(buffer, FindLineStart(buffer, buffer->cursor));

  InsertCharAtCursor(buffer, '\n');
  for (int i = 0; i < offset; i++)
    InsertCharAtCursor(buffer, ' ');
}

// TODO: this could be optimized, no need to traverse whole file for each selection line
i32 GetLineLength(Buffer* text, i32 line) {
  i32 currentLine = 0;
  i32 currentLineLength = 0;
  for (i32 i = 0; i < text->size; i++) {
    if (text->content[i] == '\n') {
      if (currentLine == line)
        return currentLineLength + 1;

      currentLine++;
      currentLineLength = 0;
    } else {
      currentLineLength++;
    }
  }
  return currentLineLength;
}

//
// Primitive tokenizer for C
//
//
typedef enum AppTokenType {
  Text,
  Preprocessor,
  Type,
  Keyword,
  Comment,
  StringLiteral,
  CharLiteral
} AppTokenType;

typedef struct Token {
  int start;
  int len;
  AppTokenType type;
} Token;

Token* tokens;
int tokensCount;

int IsKeyword(const char* str, int len) {
  const char* keywords[] = {"for",   "if",   "else", "typedef", "struct", "inline", "switch",
                            "break", "case", "enum", "while",   "return", "const",  "sizeof"};
  for (int i = 0; i < sizeof(keywords) / sizeof(*keywords); i++) {
    if (strncmp(str, keywords[i], len) == 0 && keywords[i][len] == '\0') {
      return 1;
    }
  }
  return 0;
}

int IsType(const char* str, int len) {
  const char* types[] = {"int",
                         "void",
                         "float",
                         "char",
                         "i32",
                         "Key",
                         "u8",
                         "Rect",
                         "FontData",
                         "Arena",
                         "MonochromeTexture",
                         "BITMAPINFO",
                         "Spring",
                         "ChangeType",
                         "str",
                         "EdFile",
                         "HDC",
                         "MyBitmap",
                         "PAINTSTRUCT",
                         "LRESULT",
                         "UINT",
                         "WPARAM",
                         "LPARAM",
                         "HWND",
                         "MSG",
                         "u32",
                         "i64",
                         "f64",
                         "f32",
                         "u64",
                         "Buffer",
                         "SelectionRange",
                         "Res",
                         "ChangeArena",
                         "Change",
                         "CursorPos"};
  for (int i = 0; i < ArrayLength(types); i++) {
    if (strncmp(str, types[i], len) == 0 && types[i][len] == '\0') {
      return 1;
    }
  }
  return 0;
}

int IsPreprocessor(const char* str, int len) {
  const char* pre[] = {"#define", "#include", "#pragma"};
  for (int i = 0; i < sizeof(pre) / sizeof(*pre); i++) {
    if (strncmp(str, pre[i], len) == 0 && pre[i][len] == '\0') {
      return 1;
    }
  }
  return 0;
}

void SplitIntoTokens(Buffer* buffer) {
  tokensCount = 0;
  int i = 0;
  char* src = buffer->content;
  int size = buffer->size;

  while (i < size) {
    // Skip whitespace
    while (i < size && isspace(src[i]))
      i++;

    if (i >= size)
      break;

    if (src[i] == '/' && src[i + 1] == '/') {
      int commentStart = i;
      while (src[i] != '\n')
        i++;
      tokens[tokensCount++] =
          (Token){.start = commentStart, .len = i - commentStart, .type = Comment};

      i++;
    } else if (src[i] == '\'') {
      int commentStart = i;
      i++;

      while (i < size && src[i] != '\n') {
        if (src[i] == '\'' && src[i - 1] != '\\')
          break;
        i++;
      }

      tokens[tokensCount++] =
          (Token){.start = commentStart, .len = i - commentStart + 1, .type = CharLiteral};

      i++;
    } else if (src[i] == '"') {
      int commentStart = i;
      i++;

      while (i < size && src[i] != '\n') {
        if (src[i] == '\"' && src[i - 1] != '\\')
          break;
        i++;
      }
      tokens[tokensCount++] =
          (Token){.start = commentStart, .len = i - commentStart + 1, .type = StringLiteral};

      i++;
    } else {
      int start = i;

      // Simple word token: letters, digits, #, _
      while (i < size && (isalnum(src[i]) || src[i] == '_' || src[i] == '#')) {
        i++;
      }

      int len = i - start;
      if (len > 0) {
        AppTokenType type = Text;
        if (IsPreprocessor(&src[start], len))
          type = Preprocessor;
        else if (IsKeyword(&src[start], len))
          type = Keyword;
        else if (IsType(&src[start], len))
          type = Type;

        if (type == Type && i < size && src[i] == '*') {
          i++;
          len++;
        }

        tokens[tokensCount++] = (Token){.start = start, .len = len, .type = type};
      } else {
        tokens[tokensCount++] = (Token){.start = start, .len = len, .type = Text};
        i++;
      }
    }

    // You can extend here to handle punctuation tokens (e.g. '(', ';', etc.)
  }
}
