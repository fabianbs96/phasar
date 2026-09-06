extern void srcsink(int &);
extern void sink(int);

int main(int argc, char *argv[]) {
  // The configuration is provided as callback
  //
  // PHASAR_DECLARE_FUN_AS_SINK(srcsink, 0);
  // PHASAR_DECLARE_FUN_AS_SINK(sink, 0);
  // PHASAR_DECLARE_FUN_AS_SOURCE(srcsink, false, 0);

  int x = 42;
  int y = 24;

  srcsink(x);
  srcsink(y);

  srcsink(x); // leak
  sink(y);    // leak
}

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ide-xtaint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint21_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ide-xtaint
// ide-xtaint: A LLVM-based static analysis framework

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint21_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-taint
// ifds-taint: A LLVM-based static analysis framework

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-fieldsens-taint --module %S/../../../build/test/llvm_test_code/xtaint/xtaint21_cpp_dbg.ll | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-fieldsens-taint
// ifds-fieldsens-taint: A LLVM-based static analysis framework
