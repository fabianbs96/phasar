#include <stdlib.h>

int main() {

  for (int i = 0; i < 10; ++i) {
    void *p = malloc(4);
    free(p); // no leak
  }

  return 0;
}
