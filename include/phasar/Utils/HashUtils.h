/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_UTILS_HASHUTILS_H
#define PHASAR_UTILS_HASHUTILS_H

#include "phasar/Utils/TypeTraits.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/ADT/DenseMapInfo.h"

#include <cstddef>
#include <utility>

namespace psr {

template <typename T>
concept IsDefaultHashable = is_llvm_hashable_v<T> || is_std_hashable_v<T>;

struct DefaultHasher {
  template <typename T> static auto getHashValue(const T &Val) noexcept {
    if constexpr (has_getHashCode_v<T>) {
      return Val.getHashCode();
    } else if constexpr (has_llvm_dense_map_info<T>) {
      return llvm::DenseMapInfo<T>::getHashValue(Val);
    } else if constexpr (is_llvm_hashable_v<T>) {
      using llvm::hash_value;
      return hash_value(Val);
    } else {
      return std::hash<T>{}(Val);
    }
  }

  PSR_CXX23_STATIC auto
  operator()(const auto &Val) PSR_PRECXX23_CONST noexcept {
    return getHashValue(Val);
  }
};

struct PairHash {
  template <typename T, typename U>
  inline PSR_CXX23_STATIC size_t operator()(const std::pair<T, U> &Pair)
      PSR_PRECXX23_CONST noexcept;

  template <typename... Ts>
  inline PSR_CXX23_STATIC size_t operator()(const std::tuple<Ts...> &Tuple)
      PSR_PRECXX23_CONST noexcept;
};

using DefaultHashFn = psr::Overloaded<DefaultHasher, PairHash>;

inline constexpr DefaultHashFn DefaultHash{};

template <typename T, typename U>
inline size_t
PairHash::operator()(const std::pair<T, U> &Pair) PSR_PRECXX23_CONST noexcept {
  if constexpr (has_llvm_dense_map_info<T> && has_llvm_dense_map_info<U>) {
    return llvm::DenseMapInfo<std::pair<T, U>>::getHashValue(Pair);
  } else if (is_llvm_hashable_v<T> && is_llvm_hashable_v<U>) {
    return llvm::hash_value(Pair);
  } else {
    std::pair HashPair = {DefaultHash(Pair.first), DefaultHash(Pair.second)};
    return llvm::DenseMapInfo<decltype(HashPair)>::getHashValue(HashPair);
  }
}

template <typename... Ts>
inline size_t PairHash::operator()(const std::tuple<Ts...> &Tuple)
    PSR_PRECXX23_CONST noexcept {
  if constexpr ((has_llvm_dense_map_info<Ts> && ...)) {
    return llvm::DenseMapInfo<std::tuple<Ts...>>::getHashValue(Tuple);
  } else if ((is_llvm_hashable_v<Ts> && ...)) {
    return llvm::hash_value(Tuple);
  } else {
    return
        []<size_t... I>(const auto &Tuple,
                        std::index_sequence<I...>) PSR_CXX23_STATIC noexcept {
          std::tuple HashTup = {DefaultHash(std::get<I>(Tuple))...};
          return llvm::DenseMapInfo<decltype(HashTup)>::getHashValue(HashTup);
        }(Tuple, std::make_index_sequence<sizeof...(Ts)>());
  }
}

} // namespace psr

#endif // PHASAR_UTILS_HASHUTILS_H
