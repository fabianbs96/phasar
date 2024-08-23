/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#pragma once

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
class Value;
} // namespace llvm

namespace psr::analysis::call_graph {
struct TypeAssignmentGraph;
struct SCCHolder;
struct SCCCallers;
struct SCCOrder;

struct TypeAssignment {
  llvm::SmallVector<llvm::SmallDenseSet<const llvm::Value *>, 0> TypesPerSCC;

  LLVM_LIBRARY_VISIBILITY void print(llvm::raw_ostream &OS,
                                     const TypeAssignmentGraph &TAG,
                                     const SCCHolder &SCCs);
};

[[nodiscard]] LLVM_LIBRARY_VISIBILITY TypeAssignment
propagateTypes(const TypeAssignmentGraph &TAG, const SCCHolder &SCCs,
               const SCCCallers &Deps, const SCCOrder &Order);

} // namespace psr::analysis::call_graph
