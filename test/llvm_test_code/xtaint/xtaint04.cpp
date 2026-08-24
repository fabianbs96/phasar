// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint04_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint04.cpp:5:3:
// CHECK: /xtaint/xtaint04.cpp:6:3:

void print([[clang::annotate("psr.sink")]] int) {}

void bar(int *arr) {
  print(arr[0]);
  print(arr[1]);
}

void foo(int x) {
  int array[2];
  array[1] = x;
  bar(array);
}

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {
  foo(argc);
}
