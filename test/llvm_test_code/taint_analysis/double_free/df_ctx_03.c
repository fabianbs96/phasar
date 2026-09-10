#include <stdlib.h>

void inner(int *p) { free(p); }
void outer(int *p) { inner(p); }

int main() {
  int *foo = (int *)malloc(32);
  free(foo);
  outer(foo);

  return 0;
}
