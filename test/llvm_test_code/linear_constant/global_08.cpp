int g = 1;

int baz(int c) { // clang-format off
  return g + c;
} // clang-format on

int bar(int b) { // clang-format off
  return baz(b + 1);
} // clang-format on

int foo(int a) { // clang-format off
  return bar(a + 1);
} // clang-format on

int main() {
  g += 1;
  int i = 0;
  i = foo(1);
  return 0;
}
