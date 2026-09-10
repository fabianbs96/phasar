#include <stdlib.h>

int *g;

void v(void) { free(g); }

int main() {
  int *x = (int *)malloc(32);

  if (rand()) {
    g = x;
  }

  free(x);
  v();
  return 0;
}
