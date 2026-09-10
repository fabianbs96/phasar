#include <stdlib.h>

int main() {
  int *Arr[] = {
      (int *)malloc(4),
      (int *)malloc(4),
  };

  for (int i = 0; i < 2; ++i) {
    free(Arr[i]); // no vulnerability -- fp
  }

  return 0;
}
