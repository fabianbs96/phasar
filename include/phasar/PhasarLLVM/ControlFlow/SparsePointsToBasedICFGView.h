#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/ControlFlow/CallGraph.h"
#include "phasar/ControlFlow/ICFGBase.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseCFGCache.h"
#include "phasar/PhasarLLVM/ControlFlow/SparsePointsToBasedCFGProvider.h"
#include "phasar/PhasarLLVM/ControlFlow/SparsePointsToControlFlow.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToInfo.h"
#include "phasar/PhasarLLVM/Utils/LLVMBasedContainerConfig.h"

#include <cassert>

namespace psr {

template <detail::HasMayPointsTo PTRef>
  requires detail::HasAsAbstractObject<PTRef>
class SparsePointsToBasedICFGView;

template <typename PTRef>
struct CFGTraits<SparsePointsToBasedICFGView<PTRef>> : CFGTraits<LLVMBasedCFG> {
};

/// \brief Like SparseLLVMBasedICFGView, sparsified via points-to
/// information (PTRef) instead of alias information. Non-owning: wraps an
/// already existing LLVMBasedICFG.
template <detail::HasMayPointsTo PTRef>
  requires detail::HasAsAbstractObject<PTRef>
class SparsePointsToBasedICFGView
    : public LLVMBasedCFG,
      public ICFGBase<SparsePointsToBasedICFGView<PTRef>>,
      public SparsePointsToBasedCFGProvider<SparsePointsToBasedICFGView<PTRef>,
                                            PTRef> {
  friend ICFGBase<SparsePointsToBasedICFGView>;
  friend SparsePointsToBasedCFGProvider<SparsePointsToBasedICFGView, PTRef>;

public:
  using typename LLVMBasedCFG::f_t;
  using typename LLVMBasedCFG::n_t;
  using o_t = typename PTRef::o_t;

  explicit SparsePointsToBasedICFGView(const LLVMBasedICFG *ICF, PTRef PT)
      : ICF(ICF), PointsToInfo(PT) {}

  operator const LLVMBasedICFG &() const noexcept { return *ICF; }

  template <typename D>
  [[nodiscard]] n_t advanceToNextUser(n_t Succ, const D &Fact) const {
    return SparseCFGCacheInst.advanceToNextUser(Succ, Fact, PointsToInfo);
  }

private:
  using Policy = SparsePointsToControlFlow<PTRef>;

  [[nodiscard]] FunctionRange getAllFunctionsImpl() const {
    return ICF->getAllFunctions();
  }
  [[nodiscard]] f_t getFunctionImpl(llvm::StringRef Fun) const {
    return ICF->getFunction(Fun);
  }

  [[nodiscard]] bool isIndirectFunctionCallImpl(n_t Inst) const {
    return ICF->isIndirectFunctionCall(Inst);
  }
  [[nodiscard]] bool isVirtualFunctionCallImpl(n_t Inst) const {
    return ICF->isVirtualFunctionCall(Inst);
  }
  [[nodiscard]] std::vector<n_t> allNonCallStartNodesImpl() const {
    return ICF->allNonCallStartNodes();
  }
  [[nodiscard]] llvm::SmallVector<n_t> getCallsFromWithinImpl(f_t Fun) const {
    return ICF->getCallsFromWithin(Fun);
  }
  [[nodiscard]] llvm::SmallVector<n_t, 2>
  getReturnSitesOfCallAtImpl(n_t Inst) const {
    return ICF->getReturnSitesOfCallAt(Inst);
  }
  void printImpl(llvm::raw_ostream &OS) const { ICF->print(OS); }
  [[nodiscard]] const CallGraph<n_t, f_t> &getCallGraphImpl() const noexcept {
    return ICF->getCallGraph();
  }

  [[nodiscard]] const SparseLLVMBasedCFG &getSparseCFGImpl(f_t Fun,
                                                           o_t Val) const {
    return SparseCFGCacheInst.getOrCreate(*this, Fun, Val, PointsToInfo);
  }

  [[nodiscard]] size_t getNumCallSitesImpl() const noexcept {
    return ICF->getNumCallSites();
  }

  const LLVMBasedICFG *ICF{};
  mutable SparseCFGCache<Policy, PTRef> SparseCFGCacheInst;
  PTRef PointsToInfo;
};

using LLVMSparsePointsToBasedICFGView =
    SparsePointsToBasedICFGView<LLVMPointsToIteratorRef>;

} // namespace psr
