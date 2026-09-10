#include <stdlib.h>
void v(int *foo) {
  free(foo); // vulnerability
}
void f(int *foo) { v(foo); }
int main() {
  int *foo = (int *)malloc(32);
  free(foo);
  f(foo);
  return 0;
}
