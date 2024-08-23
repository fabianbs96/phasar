#pragma once

#include "phasar/Utils/StableVector.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/IR/Value.h"

namespace psr::analysis::call_graph {
struct ObjectGraph;

struct AliasSets {
  using AliasSetTy = llvm::SmallDenseSet<const llvm::Value *>;

  psr::StableVector<AliasSetTy> AliasSetOwner{};
  llvm::SmallVector<AliasSetTy *, 0> AliasSetMap{};

  void print(llvm::raw_ostream &OS, const ObjectGraph &Graph) const;
};

class AliasInfo {
public:
  explicit AliasInfo(const ObjectGraph *Graph) noexcept : Graph(Graph) {
    assert(Graph != nullptr);
  }

  auto aliases() {
    return [this](const llvm::Value *Fact,
                  llvm::function_ref<void(const llvm::Value *)> WithAlias) {
      return foreachAlias(Fact, WithAlias);
    };
  };

private:
  void foreachAlias(const llvm::Value *Fact,
                    llvm::function_ref<void(const llvm::Value *)> WithAlias);

  const ObjectGraph *Graph{};
};

} // namespace psr::analysis::call_graph
