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
#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFG.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

namespace psr {

/// Builds a Sparse Value Flow Graph for a whole program or incrementally
/// per function.
///
/// Two-phase usage (build()):
///   1. All intra-procedural nodes and edges are created first.
///   2. Inter-procedural Call/Return boundary edges are added second,
///      ensuring callee nodes exist before connecting them.
///
/// Lazy usage (addFunction() + buildInterProc()):
///   The caller must ensure callees are added before inter-proc edges.
class SVFGBuilder {
public:
  explicit SVFGBuilder(const LLVMBasedICFG &ICFG, LLVMAliasInfoRef AA) noexcept;

  /// Whole-program eager construction. Calls finalize() before returning.
  [[nodiscard]] SVFG build();

  /// Adds intra-procedural nodes and edges for F. Idempotent.
  /// Inter-procedural edges for call sites within F must be added separately
  /// via buildInterProc() after all reachable callees have been added.
  void addFunction(const llvm::Function *F, SVFG &G);

  /// Adds ActualParam / FormalRet→ActualRet boundary nodes and edges for a
  /// single call site. Callee must already have been processed by addFunction.
  void buildInterProc(const llvm::CallBase *CS, const llvm::Function *Callee,
                      SVFG &G);

private:
  void buildIntraSSA(const llvm::Function *F, SVFG &G);
  void buildIndirect(const llvm::Function *F, SVFG &G);

  const LLVMBasedICFG *ICFGPtr;
  LLVMAliasInfoRef AliasAnalysis;
  llvm::SmallPtrSet<const llvm::Function *, 32> Processed;
};

} // namespace psr
