/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#pragma once

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/HashBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include "../../../../../utils/include/phasar/Utils/Compressor.h"

namespace psr {
class LLVMBasedICFG;
} // namespace psr

namespace psr::analysis::call_graph {
struct TypeAssignmentGraph;
enum class TAGNodeId : uint32_t;

enum class [[clang::enum_extensibility(open)]] SCCId : uint32_t{};

struct SCCHolder {
  llvm::SmallVector<SCCId, 0> SCCOfNode{};
  llvm::SmallVector<llvm::SmallVector<TAGNodeId, 1>> NodesInSCC{};
  size_t NumSCCs = 0;
};

struct SCCCallers {
  llvm::SmallVector<llvm::SmallDenseSet<SCCId>, 0> ChildrenOfSCC{};
  llvm::SmallVector<SCCId, 0> SCCRoots{};

  void print(llvm::raw_ostream &OS, const SCCHolder &SCCs,
             const TypeAssignmentGraph &TAG);
};

struct SCCOrder {
  llvm::SmallVector<SCCId, 0> SCCIds;
};

[[nodiscard]] LLVM_LIBRARY_VISIBILITY SCCHolder
computeSCCs(const TypeAssignmentGraph &TAG);

[[nodiscard]] LLVM_LIBRARY_VISIBILITY SCCCallers
computeSCCCallers(const TypeAssignmentGraph &TAG, const SCCHolder &SCCs);

[[nodiscard]] LLVM_LIBRARY_VISIBILITY SCCOrder
computeSCCOrder(const SCCHolder &SCCs, const SCCCallers &Callers);
} // namespace psr::analysis::call_graph

namespace llvm {
template <> struct DenseMapInfo<psr::analysis::call_graph::SCCId> {
  using SCCId = psr::analysis::call_graph::SCCId;

  static inline SCCId getEmptyKey() noexcept { return SCCId(-1); }
  static inline SCCId getTombstoneKey() noexcept { return SCCId(-2); }
  static inline auto getHashValue(SCCId Id) noexcept {
    return llvm::hash_value(uint32_t(Id));
  }
  static inline bool isEqual(SCCId L, SCCId R) noexcept { return L == R; }
};
} // namespace llvm
