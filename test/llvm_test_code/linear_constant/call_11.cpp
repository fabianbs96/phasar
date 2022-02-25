int bar(int b) { // clang-format off
  return b;
} // clang-format on

int foo(int a) { // clang-format off
  return bar(a);
} // clang-format on

int main() {
  int i;
  i = foo(2);
  return 0;
}
