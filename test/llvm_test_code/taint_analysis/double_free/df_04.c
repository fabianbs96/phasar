#include <stdlib.h>
void v(int *foo) {
  free(foo); // vulnerability
}
int main() {
  int *foo = (int *)malloc(32);
  free(foo);
  v(foo);
  return 0;
}
