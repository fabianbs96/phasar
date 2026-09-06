#include <stdlib.h>

int main() {
  void *X = malloc(32);
  free(X);
  free(X);
}

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-taint --module %S/../../../build/test/llvm_test_code/taint_analysis/double_free_01_c_dbg.ll --analysis-config %S/../../../config/double-free-config.json | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-taint
// ifds-taint: /taint_analysis/double_free_01.c:6:3:

// RUN: %S/../../../build/tools/phasar-cli/phasar-cli --data-flow-analysis=ifds-fieldsens-taint --module %S/../../../build/test/llvm_test_code/taint_analysis/double_free_01_c_dbg.ll --analysis-config ../../../config/double-free-config.json | /usr/local/llvm-16/bin/FileCheck %s -check-prefix=ifds-fieldsens-taint
// ifds-fieldsens-taint: /taint_analysis/double_free_01.c:6:3:
