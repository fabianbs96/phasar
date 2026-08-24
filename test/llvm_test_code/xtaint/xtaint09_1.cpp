// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint09_1_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint09_1.cpp:14:3:

#include <cstdio>
#include <cstdlib>

[[clang::annotate("psr.source")]] extern int source() { return 0; }
void sink([[clang::annotate("psr.sink")]] int) {}

int main() {
  auto mem = (int *)malloc(sizeof(int));
  *mem = source();

  if (rand())
    *mem = 42;

  sink(*mem);

  free(mem);
}
