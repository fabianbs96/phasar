/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_UTILS_ADDRESSTAKENFUNCTIONS_H
#define PHASAR_PHASARLLVM_UTILS_ADDRESSTAKENFUNCTIONS_H

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/TrailingObjects.h"

namespace llvm {
class Function;
} // namespace llvm

namespace psr {

class LLVMProjectIRDB;

/// A variant of F->hasAddressTaken() that is better suited for our use cases.
///
/// Especially, it filteres out global aliases.
[[nodiscard]] bool isAddressTakenFunction(const llvm::Function *F);

class AddressTakenFunctions {
public:
  using value_type = const llvm::Function *;
  using iterator = const value_type *;

  constexpr AddressTakenFunctions() noexcept = default;

  explicit AddressTakenFunctions(const LLVMProjectIRDB &IRDB);

  [[nodiscard]] iterator begin() const noexcept {
    return !Data ? nullptr : Data->getTrailingObjects<value_type>();
  }

  [[nodiscard]] iterator end() const noexcept {
    return !Data ? nullptr : begin() + Data->NumAddressTakenFunctions;
  }

  [[nodiscard]] bool isNone() const noexcept { return Data == nullptr; }

  [[nodiscard]] bool empty() const noexcept { return size() == 0; }
  [[nodiscard]] size_t size() const noexcept {
    return Data ? Data->NumAddressTakenFunctions : 0;
  }

private:
  struct DataT final : llvm::TrailingObjects<DataT, const llvm::Function *> {
    DataT(uint32_t NumATF) : NumAddressTakenFunctions(NumATF) {}

    DataT(const DataT &) = delete;
    DataT &operator=(const DataT &) = delete;
    ~DataT() = default;

    void destroyData() const noexcept;

    // Implementing the llvm::RefCountedBase interface:

    void Retain() const { // NOLINT
      ++RefCount;
    }
    void Release() const { // NOLINT
      assert(RefCount > 0 && "Reference count is already zero.");
      if (--RefCount == 0) {
        destroyData();
      }
    }

    // ---
    mutable uint32_t RefCount{};
    uint32_t NumAddressTakenFunctions{};
  };

  llvm::IntrusiveRefCntPtr<const DataT> Data{};
};
} // namespace psr

#endif // PHASAR_PHASARLLVM_UTILS_ADDRESSTAKENFUNCTIONS_H
