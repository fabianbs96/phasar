
/// TODO: Fields

#include <stdlib.h>

typedef struct _S {
  int *X;
  int *Y;
} S;

void v(S *foo) {
  free(foo->Y); // foo->X is free'd, but foo->Y is not, so this is fine
}
void f(S *foo) { v(foo); }
int main() {
  S foo = {};
  foo.X = (int *)malloc(32);
  foo.Y = (int *)malloc(32);

  free(foo.X);
  f(&foo);
  return 0;
}
