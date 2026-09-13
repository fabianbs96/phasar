#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/ControlFlow/ICFGBase.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFGViewMixin.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseCFGCache.h"
#include "phasar/PhasarLLVM/ControlFlow/SparsePointsToBasedCFGProvider.h"
#include "phasar/PhasarLLVM/ControlFlow/SparsePointsToControlFlow.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToInfo.h"
#include "phasar/Utils/Macros.h"

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
                                            PTRef>,
      public LLVMBasedICFGViewMixin<SparsePointsToBasedICFGView<PTRef>> {
  friend ICFGBase<SparsePointsToBasedICFGView>;
  friend SparsePointsToBasedCFGProvider<SparsePointsToBasedICFGView, PTRef>;

public:
  using typename LLVMBasedCFG::f_t;
  using typename LLVMBasedCFG::n_t;
  using o_t = typename PTRef::o_t;

  explicit SparsePointsToBasedICFGView(
      const LLVMBasedICFG *ICF PSR_LIFETIMEBOUND, PTRef PT PSR_LIFETIMEBOUND)
      : LLVMBasedICFGViewMixin<SparsePointsToBasedICFGView>(ICF),
        PointsToInfo(PT) {}

  template <typename D>
  [[nodiscard]] n_t advanceToNextUser(n_t Succ, const D &Fact) const {
    return SparseCFGCacheInst.advanceToNextUser(Succ, Fact, PointsToInfo);
  }

private:
  using Policy = SparsePointsToControlFlow<PTRef>;

  [[nodiscard]] const SparseLLVMBasedCFG &getSparseCFGImpl(f_t Fun,
                                                           o_t Val) const {
    return SparseCFGCacheInst.getOrCreate(*this, Fun, Val, PointsToInfo);
  }

  mutable SparseCFGCache<Policy, PTRef> SparseCFGCacheInst;
  PTRef PointsToInfo;
};

using LLVMSparsePointsToBasedICFGView =
    SparsePointsToBasedICFGView<LLVMPointsToIteratorRef>;

} // namespace psr
