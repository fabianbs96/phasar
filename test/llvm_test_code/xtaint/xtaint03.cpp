void print([[clang::annotate("psr.sink")]] int) {}

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {

  int array[2];
  array[1] = argc;

  print(array[0]);
  print(array[1]);
}

// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint03_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint03.cpp:8:3:
// CHECK: /xtaint/xtaint03.cpp:9:3:
