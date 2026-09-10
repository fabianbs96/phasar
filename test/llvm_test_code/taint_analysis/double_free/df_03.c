#include <stdlib.h>
void v(int *foo) {
  free(foo);
  free(foo); // vulnerability
}
int main() {
  int *foo = (int *)malloc(32);
  v(foo);
  return 0;
}
