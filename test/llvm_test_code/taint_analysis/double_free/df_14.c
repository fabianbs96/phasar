#include <stdlib.h>

int main() {
  int *Arr[] = {
      (int *)malloc(4),
      (int *)malloc(4),
  };

  for (int **it = Arr, **end = it + 2; it != end; ++it) {
    free(*it); // no vulnerability -- fp
  }

  return 0;
}

// RUN: %S/../../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ide-xtaint --module %S/../../../../build/test/llvm_test_code/taint_analysis/double_free/df_14_c_dbg.ll --analysis-config %S/../../../../config/double-free-config.json | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=xtaint
// xtaint: A LLVM-based static analysis framework

// RUN: %S/../../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../../build/test/llvm_test_code/taint_analysis/double_free/df_14_c_dbg.ll --analysis-config %S/../../../../config/double-free-config.json | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-taint
// ifds-taint: A LLVM-based static analysis framework

// RUN: %S/../../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-fieldsens-taint --module %S/../../../../build/test/llvm_test_code/taint_analysis/double_free/df_14_c_dbg.ll --analysis-config %S/../../../../config/double-free-config.json | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-fieldsens-taint
// ifds-fieldsens-taint: A LLVM-based static analysis framework
