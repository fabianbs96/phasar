void foo(int &x) {
  int i = 0;
  i = i + x;
}

int main() {
  int i;
  foo(i);
  return 0;
}
