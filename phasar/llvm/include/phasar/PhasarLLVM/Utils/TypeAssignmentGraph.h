#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/HashBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include "../../../../../utils/include/phasar/Utils/Compressor.h"
#include "../ControlFlow/CallGraph.h"
#include "../ControlFlow/LLVMVFTableProvider.h"
#include "../Pointer/LLVMAliasInfo.h"
#include "../TypeHierarchy/LLVMTypeHierarchy.h"

#include <optional>
#include <variant>

namespace psr {
class FilteredAliasSet;
} // namespace psr

namespace psr::analysis::call_graph {

enum class [[clang::enum_extensibility(open)]] TAGNodeId : uint32_t{};

struct Variable {
  const llvm::Value *Val;
};

struct Field {
  const llvm::Type *Base;
  size_t ByteOffset;
};

struct Return {
  const llvm::Function *Fun;
};

struct TAGNode {
  std::variant<Variable, Field, Return> Label;
};

constexpr bool operator==(Variable L, Variable R) noexcept {
  return L.Val == R.Val;
}
constexpr bool operator==(Field L, Field R) noexcept {
  return L.Base == R.Base && L.ByteOffset == R.ByteOffset;
}
constexpr bool operator==(Return L, Return R) noexcept {
  return L.Fun == R.Fun;
}
constexpr bool operator==(TAGNode L, TAGNode R) noexcept {
  return L.Label == R.Label;
}
}; // namespace psr::analysis::call_graph

namespace llvm {
template <> struct DenseMapInfo<psr::analysis::call_graph::TAGNode> {
  using TAGNode = psr::analysis::call_graph::TAGNode;
  using Variable = psr::analysis::call_graph::Variable;
  using Field = psr::analysis::call_graph::Field;
  using Return = psr::analysis::call_graph::Return;

  inline static TAGNode getEmptyKey() noexcept {
    return {Variable{llvm::DenseMapInfo<const llvm::Value *>::getEmptyKey()}};
  }
  inline static TAGNode getTombstoneKey() noexcept {
    return {
        Variable{llvm::DenseMapInfo<const llvm::Value *>::getTombstoneKey()}};
  }
  inline static bool isEqual(TAGNode L, TAGNode R) noexcept { return L == R; }
  inline static auto getHashValue(TAGNode TN) noexcept {
    if (const auto *Var = std::get_if<Variable>(&TN.Label)) {
      return llvm::hash_combine(0, Var->Val);
    }
    if (const auto *Fld = std::get_if<Field>(&TN.Label)) {
      return llvm::hash_combine(1, Fld->Base, Fld->ByteOffset);
    }
    if (const auto *Ret = std::get_if<Return>(&TN.Label)) {
      return llvm::hash_combine(2, Ret->Fun);
    }
    llvm_unreachable("All TAGNode variants should be handled already");
  }
};

template <> struct DenseMapInfo<psr::analysis::call_graph::TAGNodeId> {
  using TAGNodeId = psr::analysis::call_graph::TAGNodeId;
  inline static TAGNodeId getEmptyKey() noexcept { return TAGNodeId(-1); }
  inline static TAGNodeId getTombstoneKey() noexcept { return TAGNodeId(-2); }
  inline static bool isEqual(TAGNodeId L, TAGNodeId R) noexcept {
    return L == R;
  }
  inline static auto getHashValue(TAGNodeId TN) noexcept {
    return llvm::hash_value(uint32_t(TN));
  }
};

} // namespace llvm

namespace psr::analysis::call_graph {
struct ObjectGraph;

struct TypeAssignmentGraph {

  Compressor<TAGNode, TAGNodeId> Nodes;

  llvm::SmallVector<llvm::SmallDenseSet<TAGNodeId>, 0> Adj;
  llvm::SmallDenseMap<TAGNodeId, llvm::SmallDenseSet<const llvm::Value *>>
      TypeEntryPoints;

  [[nodiscard]] inline std::optional<TAGNodeId> get(TAGNode TN) const noexcept {
    return Nodes.getOrNull(TN);
  }

  [[nodiscard]] inline TAGNode operator[](TAGNodeId Id) const noexcept {
    return Nodes[Id];
  }

  inline void addEdge(TAGNodeId From, TAGNodeId To) {
    assert(size_t(From) < Adj.size());
    assert(size_t(To) < Adj.size());

    if (From == To)
      return;

    Adj[size_t(From)].insert(To);
  }

  void print(llvm::raw_ostream &OS);
};

[[nodiscard]] TypeAssignmentGraph computeTypeAssignmentGraph(
    const llvm::Module &Mod,
    const psr::CallGraph<const llvm::Instruction *, const llvm::Function *>
        &BaseCG,
    psr::LLVMAliasInfoRef AS, const psr::LLVMVFTableProvider &VTP);

[[nodiscard]] TypeAssignmentGraph computeTypeAssignmentGraph(
    const llvm::Module &Mod,
    const psr::CallGraph<const llvm::Instruction *, const llvm::Function *>
        &BaseCG,
    const ObjectGraph &ObjGraph, const psr::LLVMVFTableProvider &VTP);

void printNode(llvm::raw_ostream &OS, TAGNode TN);
}; // namespace psr::analysis::call_graph
