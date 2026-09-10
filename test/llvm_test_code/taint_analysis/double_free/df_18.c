#include <stdlib.h>

void *alloc() { //
  return malloc(4);
}

int main() {
  void *p = alloc();
  free(p);
  p = alloc();
  free(p);

  return 0;
}
