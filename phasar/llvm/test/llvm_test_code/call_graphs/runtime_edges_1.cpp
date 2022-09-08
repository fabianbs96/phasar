void foo() {}

void bar() {}

void (*Fptr)(void) = &foo; // NOLINT

int main(int argc, char **argv) {
  Fptr();
  return 0;
}
