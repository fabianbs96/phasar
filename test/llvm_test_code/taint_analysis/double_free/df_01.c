#include <stdlib.h>
int main() {
  int *foo = (int *)malloc(32);
  free(foo);
  free(foo); // vulnerability
  return 0;
}
