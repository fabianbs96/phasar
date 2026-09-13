/******************************************************************************
 * Copyright (c) 2023 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_UTILS_NULLABLE_H
#define PHASAR_UTILS_NULLABLE_H

#include "phasar/Utils/Macros.h"

#include <cassert>
#include <optional>
#include <type_traits>
#include <utility>

namespace psr {

template <typename T>
concept IsNullable =
    requires(T Val) { bool(Val); } && !std::is_arithmetic_v<T> &&
    !std::is_enum_v<T> && !std::is_member_pointer_v<T>;

template <typename T>
using Nullable = std::conditional_t<IsNullable<T>, T, std::optional<T>>;

template <typename T>
using NullableRef =
    std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T> *,
                       Nullable<T>>;

template <typename T>
[[nodiscard]] constexpr NullableRef<T> makeNullableRef(T &&Val) noexcept {
  if constexpr (std::is_reference_v<T>) {
    return &Val;
  } else {
    return {PSR_FWD(Val)};
  }
}

template <typename T>
  requires IsNullable<T>
[[nodiscard]] constexpr T unwrapNullable(T &&Val) noexcept {
  assert(Val && "Unwrapping null-value!");
  return std::forward<T>(Val);
}
template <typename T>
  requires(!IsNullable<T>)
[[nodiscard]] constexpr T unwrapNullable(std::optional<T> &&Val) noexcept {
  assert(Val && "Unwrapping nullopt!");
  return *std::move(Val);
}
template <typename T>
  requires(!IsNullable<T>)
[[nodiscard]] constexpr const T &
unwrapNullable(const std::optional<T> &Val) noexcept {
  assert(Val && "Unwrapping nullopt!");
  return *Val;
}
template <typename T>
  requires(!IsNullable<T>)
[[nodiscard]] constexpr T &unwrapNullable(std::optional<T> &Val) noexcept {
  assert(Val && "Unwrapping nullopt!");
  return *Val;
}

template <typename T>
[[nodiscard]] constexpr auto nullableRefToPtr(T Ref) noexcept
    -> decltype(&unwrapNullable(Ref)) {
  if (Ref) {
    return &unwrapNullable(Ref);
  }
  return nullptr;
}

} // namespace psr

#endif // PHASAR_UTILS_NULLABLE_H
