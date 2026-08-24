// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint01_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint01.cpp:12:3:

void print([[clang::annotate("psr.sink")]] int) {
  /// TODO: clang::annotate does not work with function declarations (e.g.
  /// extern functions or forward references)
}

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {
  print(argc);
}
