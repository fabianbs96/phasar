#include <stdlib.h>

void *alloc() { //
  return malloc(4);
}

int main() {

  for (int i = 0; i < 10; ++i) {
    void *p = alloc();
    free(p);
  }

  return 0;
}
