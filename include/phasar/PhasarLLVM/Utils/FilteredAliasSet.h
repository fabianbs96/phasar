/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_UTILS_FILTEREDALIASSET_H
#define PHASAR_PHASARLLVM_UTILS_FILTEREDALIASSET_H

#include "phasar/PhasarLLVM/ControlFlow/TypeAssignmentGraph.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/FunctionExtras.h"
#include "llvm/ADT/STLFunctionalExtras.h"

#include <set>

namespace llvm {
class Value;
class Instruction;
} // namespace llvm

namespace psr {

class FilteredAliasSet {
public:
  using d_t = const llvm::Value *;
  using n_t = const llvm::Instruction *;
  using container_type = std::set<d_t>;

  using alias_handler_t = llvm::function_ref<void(const llvm::Value *)>;
  using alias_info_ref_t =
      llvm::function_ref<void(const llvm::Value *, alias_handler_t)>;
  using alias_info_t =
      llvm::unique_function<void(const llvm::Value *, alias_handler_t)>;

  FilteredAliasSet(alias_info_t &&PT) noexcept : PT(std::move(PT)) {
    assert(this->PT);
  }

  explicit FilteredAliasSet(psr::LLVMAliasInfoRef AS)
      : PT([AS](const llvm::Value *Fact, auto Handler) {
          for (const auto *Alias : *AS.getAliasSet(Fact)) {
            Handler(Alias);
          }
        }) {}

  [[nodiscard]] container_type getAliasSet(d_t Val, n_t At);
  [[nodiscard]] container_type getMustAliasSet(d_t Val, n_t At) {
    return {Val};
  }

  void foreachAlias(d_t Fact, n_t At, llvm::function_ref<void(d_t)> WithAlias);
  void foreachMustAlias(d_t Fact, n_t At,
                        llvm::function_ref<void(d_t)> WithAlias) {
    WithAlias(Fact);
  }

private:
  alias_info_t PT;
};
} // namespace psr

#endif
