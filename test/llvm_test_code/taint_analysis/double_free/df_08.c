#include <stdlib.h>

int main() {
  int *foo = (int *)malloc(32);
  free(foo);
  if (rand())
    free(foo); // vulnerability
  return 0;
}
