#include <stdlib.h>
// void v(int **foo) {
//   free(*foo); // vulnerability
// }

// void v(void *dataVoidPtr) {
//   /* cast void pointer to a pointer of the appropriate type */
//   char **dataPtr = (char **)dataVoidPtr;
//   /* dereference dataPtr into data */
//   char *data = (*dataPtr);
//   /* POTENTIAL FLAW: Possibly freeing memory twice */
//   free(data);
// }

void v(void **dataVoidPtr) {
  /* cast void pointer to a pointer of the appropriate type */
  // char **dataPtr = (char **)dataVoidPtr;
  /* dereference dataPtr into data */
  // void *data = (*dataVoidPtr);
  /* POTENTIAL FLAW: Possibly freeing memory twice */
  free(*dataVoidPtr);
}

int main() {
  int *foo = (int *)malloc(32);
  free(foo);
  v(&foo);
  return 0;
}
