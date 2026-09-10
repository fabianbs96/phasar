#include <stdlib.h>

typedef struct _S {
  int *XY[2];
} S;

void v(int **foo) { //
  free(foo[-1]);
}
void f(int **foo) { v(foo); }
int main() {
  S foo = {};
  foo.XY[0] = (int *)malloc(32);
  foo.XY[1] = (int *)malloc(32);

  free(foo.XY[0]);
  f(&foo.XY[1]);
  return 0;
}
