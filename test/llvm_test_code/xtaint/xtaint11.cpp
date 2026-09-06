#include <cstdio>
#include <cstdlib>
#include <memory>

[[clang::annotate("psr.source")]] extern int source() { return 0; }
void sink([[clang::annotate("psr.sink")]] int) {}
void sanitize([[clang::annotate("psr.sanitizer")]] int &) noexcept {}

struct IntPair {
  int x;
  int y;
};

int main() {
  auto mem = std::make_unique<IntPair>();
  mem->x = source();

  // if (rand())
  //   mem->y = 42;

  sanitize(mem->x);

  sink(mem->x);
  sink(mem->y);
}

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint11_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-taint
// ifds-taint: /xtaint/xtaint11.cpp:23:3:
// ifds-taint: /xtaint/xtaint11.cpp:24:3:

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-fieldsens-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint11_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-fieldsens-taint
// ifds-fieldsens-taint: /xtaint/xtaint11.cpp:23:3:
// ifds-fieldsens-taint: /xtaint/xtaint11.cpp:24:3:
