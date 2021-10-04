/******************************************************************************
 * Copyright (c) 2021 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_IFDSIDE_PROBLEMS_EXTENDEDTAINTANALYSIS_XTAINTANALYSISBASE_H_
#define PHASAR_PHASARLLVM_IFDSIDE_PROBLEMS_EXTENDEDTAINTANALYSIS_XTAINTANALYSISBASE_H_

#include "llvm/ADT/SmallPtrSet.h"

namespace llvm {
class Value;
class Instruction;
class Function;
} // namespace llvm

namespace psr {
class TaintConfig;
} // namespace psr

namespace psr::XTaint {
class AnalysisBase {
protected:
  const TaintConfig *TSF;

  explicit AnalysisBase(const TaintConfig *TSF) noexcept;

  using SourceConfigTy = llvm::SmallPtrSet<const llvm::Value *, 4>;
  using SinkConfigTy = llvm::SmallPtrSet<const llvm::Value *, 4>;
  using SanitizerConfigTy = SinkConfigTy;

  [[nodiscard]] std::pair<SourceConfigTy, SinkConfigTy>
  getConfigurationAt(const llvm::Instruction *Inst,
                     const llvm::Instruction *Succ,
                     const llvm::Function *Callee = nullptr) const;

  [[nodiscard]] SourceConfigTy
  getSourceConfigAt(const llvm::Instruction *Inst,
                    const llvm::Instruction *Succ,
                    const llvm::Function *Callee = nullptr) const;

  [[nodiscard]] SinkConfigTy
  getSinkConfigAt(const llvm::Instruction *Inst, const llvm::Instruction *Succ,
                  const llvm::Function *Callee = nullptr) const;

  [[nodiscard]] SanitizerConfigTy
  getSanitizerConfigAt(const llvm::Instruction *Inst,
                       const llvm::Instruction *Succ,
                       const llvm::Function *Callee = nullptr) const;
};
} // namespace psr::XTaint

#endif // PHASAR_PHASARLLVM_IFDSIDE_PROBLEMS_EXTENDEDTAINTANALYSIS_XTAINTANALYSISBASE_H_