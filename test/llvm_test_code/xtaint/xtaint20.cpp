extern void srcsink(
    [[clang::annotate("psr.sink")]] [[clang::annotate("psr.source")]] int &) {}
void sink([[clang::annotate("psr.sink")]] int) {}

int main(int argc, char *argv[]) {
  int x = 42;
  int y = 24;

  srcsink(x);
  srcsink(y);

  srcsink(x); // leak
  sink(y);    // leak
}

// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint20_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint20.cpp:10:3:
// CHECK: /xtaint/xtaint20.cpp:12:3:
// CHECK: /xtaint/xtaint20.cpp:13:3:
