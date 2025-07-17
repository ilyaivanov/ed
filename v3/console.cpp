#include <stdio.h>

void repeatchar(int count, char ch) {
  for (int i = 0; i < count; i++)
    printf("%c", ch);
}

int main() {
  int size = 20;
  for (int i = 1; i <= size; i += 2) {
    repeatchar((size - i) / 2, ' ');
    repeatchar(i, '*');
    printf("\n");
  }
  return 0;
}
