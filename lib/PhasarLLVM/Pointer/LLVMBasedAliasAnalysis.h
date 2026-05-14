/******************************************************************************
 * Copyright (c) 2019 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_POINTER_LLVMBASEDALIASANALYSIS_H_
#define PHASAR_PHASARLLVM_POINTER_LLVMBASEDALIASANALYSIS_H_

#include "phasar/PhasarLLVM/Pointer/AliasAnalysisView.h"
#include "phasar/Pointer/AliasAnalysisType.h"
#include "phasar/Pointer/AliasResult.h"
#include "phasar/Utils/Fn.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Compiler.h"

namespace llvm {
class Value;
class Function;
class Instruction;
class AAResults;
} // namespace llvm

namespace psr {

class LLVMProjectIRDB;

/// \brief Wrapper over alias analyses that provide point-wise alias
/// information.
///
/// Used to construct an LLVMAliasSet.
class LLVMBasedAliasAnalysis : public AliasAnalysisView {
public:
  LLVM_LIBRARY_VISIBILITY explicit LLVMBasedAliasAnalysis(
      LLVMProjectIRDB &IRDB, bool UseLazyEvaluation,
      AliasAnalysisType PATy = AliasAnalysisType::Basic);

  LLVM_LIBRARY_VISIBILITY ~LLVMBasedAliasAnalysis() override;

private:
  LLVM_LIBRARY_VISIBILITY FunctionAliasView
  doGetAAResults(const llvm::Function *F) override {
    if (!hasAliasInfo(*F)) {
      // NOLINTNEXTLINE - FIXME when it is fixed in LLVM
      computeAliasInfo(const_cast<llvm::Function &>(*F));
    }
    return createFAView(AAInfos.lookup(F));
  };

  LLVM_LIBRARY_VISIBILITY void doErase(llvm::Function *F) noexcept override;

  LLVM_LIBRARY_VISIBILITY void doClear() noexcept override;

  LLVM_LIBRARY_VISIBILITY static AliasResult
  aliasImpl(llvm::AAResults *, const llvm::Value *, const llvm::Value *,
            const llvm::DataLayout &);

  [[nodiscard]] constexpr FunctionAliasView
  createFAView(llvm::AAResults *AAR) noexcept {
    return {AAR, fn<aliasImpl>};
  }

  [[nodiscard]] LLVM_LIBRARY_VISIBILITY bool
  hasAliasInfo(const llvm::Function &Fun) const;

  LLVM_LIBRARY_VISIBILITY void computeAliasInfo(llvm::Function &Fun);

  // -- data members
  llvm::PassBuilder PB;
  llvm::FunctionAnalysisManager FAM;
  llvm::FunctionPassManager FPM;
  llvm::DenseMap<const llvm::Function *, llvm::AAResults *> AAInfos;
};

} // namespace psr

#endif
