void print([[clang::annotate("psr.sink")]] int) {}
extern int rand(void);

void foo(int x) {
  int buf;
  int *p = &buf;
  *p = x;
  if (rand())
    *p = 0;
  else
    *p = 1;

  print(buf);
}

int main([[clang::annotate("psr.source")]] int argc, char *argv[]) {
  foo(argc);
}

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint05_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-taint
// ifds-taint: /xtaint/xtaint05.cpp:13:3:

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-fieldsens-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint05_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-fieldsens-taint
// ifds-fieldsens-taint: No leaks found!
