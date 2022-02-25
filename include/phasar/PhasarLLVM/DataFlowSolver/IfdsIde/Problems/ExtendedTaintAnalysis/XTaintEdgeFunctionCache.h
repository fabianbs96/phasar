/******************************************************************************
 * Copyright (c) 2021 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_IFDSIDE_PROBLEMS_EXTENDEDTAINTANALYSIS_XTAINTEDGEFUNCTIONCACHE_H_
#define PHASAR_PHASARLLVM_IFDSIDE_PROBLEMS_EXTENDEDTAINTANALYSIS_XTAINTEDGEFUNCTIONCACHE_H_

#include <memory>
#include <unordered_map>

#include "llvm/ADT/DenseMap.h"

#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/EdgeFunctions.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/ExtendedTaintAnalysis/EdgeDomain.h"
#include "phasar/PhasarLLVM/Utils/BasicBlockOrdering.h"

namespace psr::XTaint {

struct EdgeFunctionCache {

  /// Gets the cache-entry for the GenEdgeFunction with the given properties.
  ///
  /// CAUTION: This function is not thread-safe!
  static std::weak_ptr<EdgeFunction<EdgeDomain>> &
  GenEdgeFunction(BasicBlockOrdering &BBO, const llvm::Instruction *Inst);

  /// Gets the cache-entry for the ComposeEdgeFunction with the given
  /// properties.
  ///
  /// CAUTION: This function is not thread-safe!
  static std::weak_ptr<EdgeFunction<EdgeDomain>> &
  ComposeEdgeFunction(BasicBlockOrdering &BBO,
                      const EdgeFunction<EdgeDomain> *F,
                      const EdgeFunction<EdgeDomain> *G);

  /// Erases all entries from all managed caches.
  ///
  /// CAUTION: This function is not thread-safe!
  static void clear();
};
} // namespace psr::XTaint

#endif // PHASAR_PHASARLLVM_IFDSIDE_PROBLEMS_EXTENDEDTAINTANALYSIS_XTAINTEDGEFUNCTIONCACHE_H_
