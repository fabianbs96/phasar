void sink([[clang::annotate("psr.sink")]] int) {}
extern int rand();

int foo(int x) {
  if (rand())
    return 0;

  return foo(x);
}

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {

  int x = foo(argc);

  // here, the sanitizer cannot be skipped...
  sink(x);
}

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint18_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-taint
// ifds-taint: A LLVM-based static analysis framework

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-fieldsens-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint18_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-fieldsens-taint
// ifds-fieldsens-taint: A LLVM-based static analysis framework
