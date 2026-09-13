/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_SPARSELLVMBASEDICFG_VIEW_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_SPARSELLVMBASEDICFG_VIEW_H

#include "phasar/ControlFlow/CallGraph.h"
#include "phasar/ControlFlow/ICFGBase.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFGViewMixin.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMBasedCFGProvider.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"

#include <memory>

namespace psr {
class LLVMProjectIRDB;
class LLVMBasedICFG;
class SparseLLVMBasedCFG;
class SparseLLVMBasedICFGView;
struct SVFGCache;

template <>
struct CFGTraits<SparseLLVMBasedICFGView> : CFGTraits<LLVMBasedCFG> {};

/// \brief Similar to SparseLLVMBasedICFG; the only difference is that this one
/// *is* no LLVMBasedICFG -- it contains a pointer to an already existing one.
/// It still owns the sparse value-flow graphs.
///
/// Use this in the IDESolver or IFDSSolver to profit from the SparseIFDS or
/// SparseIDE optimization after Karakaya et al. "Symbol-Specific Sparsification
/// of Interprocedural Distributive Environment Problems"
/// <https://doi.org/10.48550/arXiv.2401.14813>
class SparseLLVMBasedICFGView
    : public LLVMBasedCFG,
      public ICFGBase<SparseLLVMBasedICFGView>,
      public SparseLLVMBasedCFGProvider<SparseLLVMBasedICFGView>,
      public LLVMBasedICFGViewMixin<SparseLLVMBasedICFGView> {
  friend ICFGBase;
  friend SparseLLVMBasedCFGProvider<SparseLLVMBasedICFGView>;

public:
  using typename LLVMBasedCFG::f_t;
  using typename LLVMBasedCFG::n_t;

  explicit SparseLLVMBasedICFGView(const LLVMBasedICFG *ICF PSR_LIFETIMEBOUND,
                                   LLVMAliasInfoRef PT PSR_LIFETIMEBOUND);

  ~SparseLLVMBasedICFGView();

  [[nodiscard]] n_t advanceToNextUser(n_t Succ, const auto &Fact) const {
    using psr::valueOf;
    return advanceToNextUserImpl(Succ, valueOf(Fact));
  }

private:
  [[nodiscard]] const SparseLLVMBasedCFG &
  getSparseCFGImpl(const llvm::Function *Fun, const llvm::Value *Val) const;

  [[nodiscard]] n_t advanceToNextUserImpl(n_t Succ, v_t Fact) const;

  std::unique_ptr<SVFGCache> SparseCFGCache;
  LLVMAliasInfoRef AliasAnalysis;
};
} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_SPARSELLVMBASEDICFG_VIEW_H
