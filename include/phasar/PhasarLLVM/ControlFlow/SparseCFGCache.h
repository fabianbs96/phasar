#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMControlFlow.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"

#include <optional>
#include <unordered_map>
#include <utility>

namespace psr {

template <typename Fact> struct FVHasher {
  auto operator()(std::pair<const llvm::Function *, Fact> FV) const noexcept {
    return llvm::hash_value(FV);
  }
};

/// \brief Generic per-(Function, Fact) cache of sparse next-user graphs.
template <typename Policy, typename InfoRef> class SparseCFGCache {
public:
  using f_t = const llvm::Function *;
  using o_t = typename Policy::o_t;
  using n_t = const llvm::Instruction *;

  [[nodiscard]] const SparseLLVMBasedCFG &
  getOrCreate(const LLVMBasedCFG &CFG, f_t Fun, o_t Val, InfoRef Info) {
    auto [It, Inserted] = Cache.try_emplace(std::make_pair(Fun, Val));
    if (Inserted) {
      buildSparseCFG(CFG, It->second.VGraph, Fun, Val, Info);
    }
    return It->second;
  }

  template <typename D>
  [[nodiscard]] n_t advanceToNextUser(n_t Succ, const D &Fact, InfoRef Info) {
    // Not memoized: the forward scan skips only very few instructions on
    // average, which is cheaper than a lookup in a multi-million-entry map.
    // On a small coreutils benchmark, it was about ~11% *faster* to skip the
    // cache.
    if constexpr (requires { Policy::advanceToNextUser(Succ, Fact, Info); }) {
      return Policy::advanceToNextUser(Succ, Fact, Info);
    } else {
      return Policy::advanceToNextUser(
          Succ, Fact, Info, getGlobalObjects(Succ->getFunction(), Info));
    }
  }

private:
  /// \brief Abstract objects of Fun's module's global values.
  [[nodiscard]] const llvm::DenseSet<o_t> &getGlobalObjects(f_t Fun,
                                                            InfoRef Info) {
    if (!GlobalObjects) {
      GlobalObjects.emplace();
      for (const auto &GV : Fun->getParent()->global_values()) {
        GlobalObjects->insert(Info.asAbstractObject(&GV));
      }
    }
    return *GlobalObjects;
  }

  void buildSparseCFG(const LLVMBasedCFG &CFG,
                      SparseLLVMBasedCFG::vgraph_t &SCFG, f_t Fun, o_t Val,
                      InfoRef Info) {
    llvm::SmallVector<std::pair<n_t, n_t>> WL;

    // -- Initialization

    const auto *Entry = &Fun->getEntryBlock().front();
#if LLVM_VERSION_MAJOR <= 18
    if (llvm::isa<llvm::DbgInfoIntrinsic>(Entry)) {
      Entry = Entry->getNextNonDebugInstruction();
    }
#endif

    for (const auto *Succ : CFG.getSuccsOf(Entry)) {
      WL.emplace_back(Entry, Succ);
    }

    // -- Fixpoint Iteration

    llvm::SmallDenseSet<n_t> Handled;

    while (!WL.empty()) {
      auto [From, To] = WL.pop_back_val();

      const auto *Curr = From;
      bool Keep;
      if constexpr (requires { Policy::shouldKeepInst(To, Val, Info); }) {
        Keep = Policy::shouldKeepInst(To, Val, Info);
      } else {
        Keep =
            Policy::shouldKeepInst(To, Val, Info, getGlobalObjects(Fun, Info));
      }
      if (Keep) {
        Curr = To;
        auto [It, Inserted] = SCFG.try_emplace(From, To);
        if (!Inserted) {
          if (It->second != To) {
            It->second = nullptr;
          }
        }
      }

      if (!Handled.insert(To).second) {
        continue;
      }

      for (const auto *Succ : CFG.getSuccsOf(To)) {
        WL.emplace_back(Curr, Succ);
      }
    }
  }

  std::unordered_map<std::pair<f_t, o_t>, SparseLLVMBasedCFG, FVHasher<o_t>>
      Cache{};
  std::optional<llvm::DenseSet<o_t>> GlobalObjects;
};

// for backwards compatibility
struct SVFGCache : SparseCFGCache<SparseLLVMControlFlow, LLVMAliasInfoRef> {};

} // namespace psr
