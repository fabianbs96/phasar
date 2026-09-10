#include <stdlib.h>

int *g;

void v(void) { free(g); }

int main() {
  g = (int *)malloc(32);

  free(g);
  v();
  return 0;
}
