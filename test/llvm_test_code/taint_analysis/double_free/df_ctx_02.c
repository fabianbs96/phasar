#include <stdlib.h>

int *id(int *p) { return p; }

int main() {
  int *foo = (int *)malloc(32);
  int *bar = (int *)malloc(32);
  free(foo);
  free(bar);

  int *x = id(foo);
  int *y = id(bar);

  free(x);
  free(y);

  return 0;
}
