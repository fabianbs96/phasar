#include <stdlib.h>

int *rec(int *p, int n) { return n > 0 ? rec(p, n - 1) : p; }

int main() {
  int *foo = (int *)malloc(32);
  free(foo);

  int *x = rec(foo, 3);
  free(x);

  return 0;
}
