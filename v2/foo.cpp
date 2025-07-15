#include <stdio.h>

struct Foo {
  int a;
};

void print() {
  int a = 2;
  if (a == 2)
    printf("hi there");
  if (a > 4) {
    printf("Hello there");
    printf("Hello there");
    printf("Hello there");
  }
  printf("Hello there");
}

void another() {
  int a = 2;
  printf("Hello there");
}
int isEven(int a, int b);

void sample() {
  if (isEven(123 + 3123123 + 4324324 + 432434 + 23123123 + 23123 + 2312312,
             1 + 123123 + 123123123 + 123123)) {
    printf("goo");
  }
  int a = 2;
}
