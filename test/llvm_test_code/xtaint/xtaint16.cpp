void sink([[clang::annotate("psr.sink")]] int) {}
extern int rand();

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {

  int x = argc;
  while (rand()) {
    x = 0;
    break;
  }

  // we can skip the sanitizer => leak here
  sink(x);
}

// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint16_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint16.cpp:13:3:
