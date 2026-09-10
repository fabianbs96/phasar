#include <stdlib.h>

void **alloc() { //
  return (void **)malloc(sizeof(void *));
}

int main() {
  void **p = alloc();
  *p = malloc(4);
  free(*p);
  *p = malloc(4);
  free(*p);
  free(p);

  return 0;
}
