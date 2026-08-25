void print([[clang::annotate("psr.sink")]] int) {}
extern int rand(void);

void foo(int x) {
  int buf;
  int *p = &buf;
  *p = x;
  if (rand())
    *p = 0;
  else
    *p = 1;

  print(*p);
}

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {
  foo(argc);
}

// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint06_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint06.cpp:13:3:
