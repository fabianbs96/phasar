#include <stdlib.h>

int main() {
  int *foo = (int *)malloc(32);
  int *bar = (int *)malloc(32);

  free(bar);

  if (rand())
    foo = bar;

  free(foo); // vulnerability
  return 0;
}
