[[clang::annotate("psr.source")]] extern int source() { return 0; }
void sink([[clang::annotate("psr.sink")]] int) {}
void sanitize([[clang::annotate("psr.sanitizer")]] int *) noexcept {}

struct DoubleIntPair {
  double d;
  int i;
};

int main() {
  DoubleIntPair dip = {3.1415926, source()};

  auto x = dip.i;
  sanitize(&dip.i);

  sink(dip.i);
  sink(x);
}

// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint13_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint13.cpp:16:3:
// CHECK: /xtaint/xtaint13.cpp:17:3:
