/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
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

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/Support/Compiler.h"

#include <functional>

namespace psr {
/// \brief A wrapper over a pointer that cannot be nullptr. Similar to
/// std::reference_wrapper, but provides a pointer-like interface (operators *
/// and ->).
template <typename T> class NonNullPtr {
public:
  /// Creates a NonNullPtr from a raw pointer.
  ///
  /// \attention Asserts-out, if Ptr is nullptr
  NonNullPtr(T *Ptr) : Ptr(&psr::assertNotNull(Ptr)) {}

  /// Creates a NonNullPtr by capturing the given reference. This ctor cannot
  /// fail.
  constexpr explicit NonNullPtr(T &Ref) noexcept : Ptr(&Ref) {}

  /// Creates a NonNullPtr by capturing the given reference_wrapper. This ctor
  /// cannot fail.
  PSR_CXX20_CONSTEXPR NonNullPtr(std::reference_wrapper<T> RW) noexcept
      : Ptr(&RW.get()) {}

  [[nodiscard]] constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE
      LLVM_ATTRIBUTE_RETURNS_NONNULL T *
      get() const noexcept {
    return Ptr;
  }

  [[nodiscard]] constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE T &
  operator*() const noexcept {
    return *get();
  }

  [[nodiscard]] constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE
      LLVM_ATTRIBUTE_RETURNS_NONNULL T *
      operator->() const noexcept {
    return get();
  }

  template <typename U>
  [[nodiscard]] friend constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE bool
  operator==(NonNullPtr This, NonNullPtr<U> Other) noexcept {
    return This.Ptr == Other.Ptr;
  }

  template <typename U>
  [[nodiscard]] friend constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE bool
  operator==(NonNullPtr This, U *Other) noexcept {
    return This.Ptr == Other;
  }

  template <typename U>
  [[nodiscard]] friend constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE bool
  operator==(U *Other, NonNullPtr This) noexcept {
    return This.Ptr == Other;
  }

  template <typename U>
  [[nodiscard]] friend constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE bool
  operator!=(NonNullPtr This, NonNullPtr<U> Other) noexcept {
    return !(This == Other);
  }

  template <typename U>
  [[nodiscard]] friend constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE bool
  operator!=(NonNullPtr This, U *Other) noexcept {
    return !(This == Other);
  }

  template <typename U>
  [[nodiscard]] friend constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE bool
  operator!=(U *Other, NonNullPtr This) noexcept {
    return !(This == Other);
  }

  friend constexpr auto hash_value(NonNullPtr NP) noexcept {
    return llvm::hash_value(NP.Ptr);
  }

private:
  T *Ptr{};
};

} // namespace psr

namespace std {
template <typename T> struct hash<psr::NonNullPtr<T>> {
  constexpr size_t operator()(psr::NonNullPtr<T> NP) const noexcept {
    return hash_value(NP);
  }
};
} // namespace std

namespace llvm {
template <typename T> struct DenseMapInfo<psr::NonNullPtr<T>> {
  using value_type = psr::NonNullPtr<T>;

  static constexpr value_type getEmptyKey() noexcept {
    return DenseMapInfo<T *>::getEmptyKey();
  }
  static constexpr value_type getTombstoneKey() noexcept {
    return DenseMapInfo<T *>::getTombstoneKey();
  }
  static constexpr auto getHashValue(value_type NP) noexcept {
    return DenseMapInfo<T *>::getHashValue(NP.get());
  }
  static constexpr bool isEqual(value_type L, value_type R) noexcept {
    return L == R;
  }
};
} // namespace llvm

#endif // PHASAR_UTILS_NONNULLPOINTER_H
