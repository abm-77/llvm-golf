#include <stdio.h>

// compile with:
// clang-18 -O0 -Xclang -disable-O0-optnone -emit-llvm -S test.c
//
// run mem2reg with:
// opt-18 -passes="mem2reg" -o test.ll test.ll
//
// run with:
// opt-18 -load-pass-plugin=./build/loop-opt/LoopPass.so
// -passes="custom-loop-pass" -disable-output test.ll

void do_smth(int c, int d) {
  int a = 10;
  int b = 20;

  for (int i = 0; i < 10; ++i) {
    int c = a * b + c / d;
  }
}

int foo(int a, int b, int c) {
  int result = 123 + a;

  if (a > 0) {
    int d = a * b;
    int e = b / c;
    if (d == e) {
      int f = d * e;
      result = result - 2 * f;
    } else {
      int g = 987;
      result = g * c * e;
    }
  } else {
    result = 321;
  }

  return result;
}

int main(void) {
  do_smth(3, 4);
  do_smth(5, 6);
  do_smth(7, 8);

  return 0;
}
