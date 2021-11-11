#ifndef PHASAR_PHASARLLVM_PASSES_RUNTIMEINFORMATION_FUNCTIONANNOTATIONPASS_H_
#define PHASAR_PHASARLLVM_PASSES_RUNTIMEINFORMATION_FUNCTIONANNOTATIONPASS_H_

#include "llvm/IR/PassManager.h"

namespace llvm {
class LLVMContext;
class Module;
class AnalysisUsage;
} // namespace llvm

namespace psr {
/**
 * This class uses the Module Pass Mechanism of LLVM to annotate every
 * Function of a Module with an unique ID.
 *
 * This pass obviously modifies the analyzed Module, but preserves the
 * CFG.
 *
 * @brief Annotates every Function with a unique ID.
 */

class FunctionAnnotationPass
    : public llvm::AnalysisInfoMixin<FunctionAnnotationPass> {
private:
  friend llvm::AnalysisInfoMixin<FunctionAnnotationPass>;
  static llvm::AnalysisKey Key;
  static size_t UniqueFunctionId;

public:
  explicit FunctionAnnotationPass();

  static llvm::PreservedAnalyses run(llvm::Module &M,
                                     llvm::ModuleAnalysisManager &AM);

  static bool isRequired() { return true; }

  /**
   * @brief Resets the global ID - only used for unit testing!
   */
  static void resetFunctionID();
};
} // namespace psr

#endif