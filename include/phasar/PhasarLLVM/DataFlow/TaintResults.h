#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"

#include <concepts>
#include <functional>
#include <map>
#include <set>

namespace psr {
class TaintResults {
public:
  struct InsertResult {
    bool second{}; // NOLINT(readability-identifier-naming)

    explicit constexpr operator bool() const noexcept { return second; }
  };

  class SetWrapper : public llvm::TinyPtrVector<const llvm::Value *> {
  public:
    InsertResult insert(const llvm::Value *Fact) {
      auto &Into = *this;
      if (llvm::is_contained(Into, Fact)) {
        return {false};
      }
      Into.push_back(Fact);
      return {true};
    }

    [[nodiscard]] bool contains(const llvm::Value *Fact) const noexcept {
      return llvm::is_contained(*this, Fact);
    }

    [[nodiscard]] bool operator==(const SetWrapper &Other) const noexcept {
      return equalsImpl(Other);
    }

    // For testing
    [[nodiscard]] bool
    operator==(const std::set<const llvm::Value *> &Other) const noexcept {
      return equalsImpl(Other);
    }

  private:
    [[nodiscard]] bool equalsImpl(const auto &Other) const noexcept {
      if (size() != Other.size()) {
        return false;
      }

      for (const auto *Elem : *this) {
        if (!Other.contains(Elem)) {
          return false;
        }
      }
      return true;
    }
  };

  SetWrapper &operator[](const llvm::Instruction *Inst) { return Leaks[Inst]; }

  InsertResult insert(const llvm::Instruction *Inst, const llvm::Value *Fact) {
    return Leaks[Inst].insert(Fact);
  }

  [[nodiscard]] auto begin() const noexcept { return Leaks.begin(); }
  [[nodiscard]] auto end() const noexcept { return Leaks.end(); }
  [[nodiscard]] auto find(const llvm::Instruction *Inst) const noexcept {
    return Leaks.find(Inst);
  }

  [[nodiscard]] bool empty() const noexcept { return Leaks.empty(); }
  [[nodiscard]] size_t size() const noexcept { return Leaks.size(); }

  friend void erase_if( // NOLINT(readability-identifier-naming)
      TaintResults &Res,
      std::predicate<const llvm::Instruction *, TaintResults::SetWrapper &> auto
          EraseCond) noexcept {
    for (auto IIt = Res.Leaks.begin(), End = Res.Leaks.end(); IIt != End;) {
      auto It = IIt++;
      const auto &[LeakInst, LeakFacts] = *It;
      if (std::invoke(EraseCond, LeakInst, LeakFacts)) {
        Res.Leaks.erase(It);
      }
    }
  }

  [[nodiscard]] bool operator==(const TaintResults &Other) const noexcept {
    return Leaks == Other.Leaks;
  }

  // For testing
  [[nodiscard]] bool operator==(
      const std::map<const llvm::Instruction *, std::set<const llvm::Value *>>
          &Other) const noexcept {
    if (Other.size() != Leaks.size()) {
      return false;
    }

    for (const auto &[Inst, Facts] : Other) {
      auto It = Leaks.find(Inst);
      if (It == Leaks.end()) {
        return false;
      }

      if (It->second != Facts) {
        return false;
      }
    }
    return true;
  }

private:
  llvm::DenseMap<const llvm::Instruction *, SetWrapper> Leaks{};
};

} // namespace psr
