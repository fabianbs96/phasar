#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/ControlFlow/SparseCFGProvider.h"
#include "phasar/Pointer/PointsToIterator.h"

namespace llvm {
class Function;
} // namespace llvm

namespace psr {

/// \brief Like SparseLLVMBasedCFGProvider, fact domain is PTRef::o_t.
template <typename Derived, detail::HasMayPointsTo PTRef>
using SparsePointsToBasedCFGProvider =
    SparseCFGProvider<Derived, const llvm::Function *, typename PTRef::o_t>;

} // namespace psr
