
/// TODO: Fields

#include <stdlib.h>

typedef struct _S {
  int *X;
  int *Y;
} S;

void v(S *foo) { //
  free(foo->Y);
}
void f(S *foo) { v(foo); }
int main() {
  S foo = {};
  foo.X = (int *)malloc(32);
  foo.Y = (int *)malloc(32);

  free(foo.Y);
  f(&foo);
  return 0;
}
