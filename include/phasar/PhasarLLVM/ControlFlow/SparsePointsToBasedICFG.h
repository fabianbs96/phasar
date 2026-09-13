#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseCFGCache.h"
#include "phasar/PhasarLLVM/ControlFlow/SparsePointsToBasedCFGProvider.h"
#include "phasar/PhasarLLVM/ControlFlow/SparsePointsToControlFlow.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToInfo.h"

namespace psr {
class DIBasedTypeHierarchy;

/// \brief Like SparseLLVMBasedICFG, sparsified via points-to information
/// (PTRef) instead of alias information.
template <detail::HasMayPointsTo PTRef>
  requires detail::HasAsAbstractObject<PTRef>
class SparsePointsToBasedICFG
    : public LLVMBasedICFG,
      public SparsePointsToBasedCFGProvider<SparsePointsToBasedICFG<PTRef>,
                                            PTRef> {
  friend SparsePointsToBasedCFGProvider<SparsePointsToBasedICFG, PTRef>;

public:
  using typename LLVMBasedICFG::f_t;
  using typename LLVMBasedICFG::n_t;
  using o_t = typename PTRef::o_t;

  /// Creates an ICFG with an already given call-graph
  explicit SparsePointsToBasedICFG(CallGraph<n_t, f_t> CG,
                                   LLVMProjectIRDB *IRDB, PTRef PT)
      : LLVMBasedICFG(std::move(CG), IRDB), PointsToInfo(PT) {}

  explicit SparsePointsToBasedICFG(LLVMProjectIRDB *IRDB,
                                   const CallGraphData &SerializedCG, PTRef PT)
      : LLVMBasedICFG(IRDB, SerializedCG), PointsToInfo(PT) {}

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

using LLVMSparsePointsToBasedICFG =
    SparsePointsToBasedICFG<LLVMPointsToIteratorRef>;

} // namespace psr
