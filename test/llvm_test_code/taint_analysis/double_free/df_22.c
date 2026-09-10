#include <stdlib.h>

int *v(int *p) { return p; }

int main() {
  int *foo = (int *)malloc(32);
  free(foo);
  int *x = v(foo);

  int a = 42;
  int *y = v(&a);

  free(x);
  free(y);

  return 0;
}
