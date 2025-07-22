#include <stdio.h>

void foo() {
  printf("hi there");
}
void bar() {
  printf("bar");
}
int main() {
  int a = 2;
  foo();
  bar();
  printf("hi there %d", a);
  return 0;
}
