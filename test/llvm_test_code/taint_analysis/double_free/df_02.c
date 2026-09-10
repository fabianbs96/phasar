#include <stdlib.h>
void v() {
  int *foo = (int *)malloc(32);
  free(foo);
  free(foo); // vulnerability
}
int main() {
  v();
  return 0;
}
