#include <stdlib.h>

int main() {
  int *Arr[] = {
      (int *)malloc(4),
      (int *)malloc(4),
  };

  for (int **it = Arr, **end = it + 2; it != end; ++it) {
    free(*it); // no vulnerability -- fp
  }

  return 0;
}
