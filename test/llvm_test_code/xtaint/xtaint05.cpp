// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint05_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint05.cpp:14:3:

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

  print(buf);
}

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {
  foo(argc);
}
