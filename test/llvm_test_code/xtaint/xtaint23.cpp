// RUN: phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint23_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s
// CHECK: /xtaint/xtaint23.cpp:17:5:

void print([[clang::annotate("psr.sink")]] int) {}

struct iterator {
  int *it{};

  void next() { //
    ++it;
  }
};

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {
  int arr[10]{};
  arr[4] = argc;

  for (iterator it = {arr}, end = {arr + 10}; it.it != end.it; it.next()) {
    print(*it.it);
  }
}
