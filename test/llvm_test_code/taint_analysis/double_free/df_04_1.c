#include <stdlib.h>
void v(int *foo) {
  free(&foo); // no vulnerability
  free(foo);  // vulnerability
}
int main() {
  int *foo = (int *)malloc(32);
  free(foo);
  v(foo);
  return 0;
}
