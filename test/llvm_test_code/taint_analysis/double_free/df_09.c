#include <stdlib.h>

int main() {
  int *foo = (int *)malloc(32);
  if (rand())
    free(foo);

  free(foo); // vulnerability
  return 0;
}
