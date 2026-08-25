void sink([[clang::annotate("psr.sink")]] int) {}
extern int rand();

int foo(int x) {
  while (rand()) {
    x = 0;
    break;
  }
  return x;
}

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {

  int x = foo(argc);

  // we can skip the sanitizer => leak here
  sink(x);
}

// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint17_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint17.cpp:17:3:
