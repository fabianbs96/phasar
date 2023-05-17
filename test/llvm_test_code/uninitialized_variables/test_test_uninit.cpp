int foo(int i) {
  int j = i + 1;
  return j;
}

int bar() {
  int j;
  return j;
}

int main() {
  int i;
  int f = 0;

  for (int n = 0; n < 3; n++) 
  {
    f += foo(i);
  }

  f = 0;
  f += bar();

  return 0;
}
