void print([[clang::annotate("psr.sink")]] int) {}

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {
  int arr[10]{};
  arr[4] = argc;

  for (int *it = arr, *end = arr + 10; it != end; ++it) {
    print(*it);
  }
}

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ide-xtaint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint22_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ide-xtaint
// ide-xtaint: A LLVM-based static analysis framework

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint22_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-taint
// ifds-taint: /xtaint/xtaint22.cpp:8:5:

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-fieldsens-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint22_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-fieldsens-taint
// ifds-fieldsens-taint: /xtaint/xtaint22.cpp:8:5:
