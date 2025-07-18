#include <stdio.h>

void p(int c, char ch) {
  for (int i = 0; i < c; i++)
    printf("%c", ch);
}
int main() {
  p(12, '*');
  //  printf("hello world");
  return 0;
}
