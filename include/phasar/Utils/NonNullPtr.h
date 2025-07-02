/******************************************************************************
 * Copyright (c) 2023 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_UTILS_NONNULLPOINTER_H
#define PHASAR_UTILS_NONNULLPOINTER_H

#include "phasar/Utils/Macros.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/Support/Compiler.h"

#include <functional>

namespace psr {
/// \brief A wrapper over a pointer that cannot be nullptr. Similar to
/// std::reference_wrapper, but provides a pointer-like interface (operators *
/// and ->).
template <typename T> class NonNullPtr : private std::reference_wrapper<T> {
  using base_t = std::reference_wrapper<T>;

public:
  NonNullPtr(T *Ptr) : base_t(psr::assertNotNull(Ptr)) {}
  PSR_CXX20_CONSTEXPR explicit NonNullPtr(T &Ref) noexcept : base_t(Ref) {}
  PSR_CXX20_CONSTEXPR NonNullPtr(std::reference_wrapper<T> RW) noexcept
      : base_t(RW) {}

  [[nodiscard]] PSR_CXX20_CONSTEXPR
      LLVM_ATTRIBUTE_ALWAYS_INLINE LLVM_ATTRIBUTE_RETURNS_NONNULL T *
      get() const noexcept {
    return &this->base_t::get();
  }

  [[nodiscard]] PSR_CXX20_CONSTEXPR LLVM_ATTRIBUTE_ALWAYS_INLINE T &
  operator*() const noexcept {
    return *get();
  }

  [[nodiscard]] PSR_CXX20_CONSTEXPR
      LLVM_ATTRIBUTE_ALWAYS_INLINE LLVM_ATTRIBUTE_RETURNS_NONNULL T *
      operator->() const noexcept {
    return get();
  }
};

} // namespace psr

#endif // PHASAR_UTILS_NONNULLPOINTER_H
