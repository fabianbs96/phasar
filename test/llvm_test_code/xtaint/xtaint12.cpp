// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint12_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint12.cpp:19:3:

[[clang::annotate("psr.source")]] extern int source() { return 0; }
void sink([[clang::annotate("psr.sink")]] int) {}
void sanitize([[clang::annotate("psr.sanitizer")]] int &) noexcept {}

struct IntPair {
  int x;
  int y;
};

int *getPtr(int **pptaint) { return *pptaint; }

int main() {
  int taint = source();
  int *ptaint = &taint;

  sanitize(*getPtr(&ptaint));

  sink(*getPtr(&ptaint));
}
