#include <stdlib.h>

void *alloc() { //
  return malloc(4);
}

int main() {
  void *p1 = alloc();
  void *p2 = alloc();

  free(p1);
  free(p2);
  return 0;
}
