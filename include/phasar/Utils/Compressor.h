/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#ifndef PHASAR_UTILS_COMPRESSOR_H
#define PHASAR_UTILS_COMPRESSOR_H

#include "phasar/Utils/ByRef.h"
#include "phasar/Utils/TypeTraits.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/SmallVector.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <type_traits>

namespace psr {
template <typename T, typename Id = uint32_t, typename Enable = void>
class Compressor;

template <typename T, typename Id>
class Compressor<T, Id, std::enable_if_t<CanEfficientlyPassByValue<T>>> {
public:
  void reserve(size_t Capacity) {
    assert(Capacity <= UINT32_MAX);
    ToInt.reserve(Capacity);
    FromInt.reserve(Capacity);
  }

  Id getOrInsert(T Elem) {
    auto [It, Inserted] = ToInt.try_emplace(Elem, Id(ToInt.size()));
    if (Inserted) {
      FromInt.push_back(Elem);
    }
    return It->second;
  }

  std::pair<Id, bool> insert(T Elem) {
    auto [It, Inserted] = ToInt.try_emplace(Elem, Id(ToInt.size()));
    if (Inserted) {
      FromInt.push_back(Elem);
    }
    return {It->second, Inserted};
  }

  [[nodiscard]] std::optional<Id> getOrNull(T Elem) const {
    if (auto It = ToInt.find(Elem); It != ToInt.end()) {
      return It->second;
    }
    return std::nullopt;
  }

  [[nodiscard]] Id get(T Elem) const {
    auto It = ToInt.find(Elem);
    assert(It != ToInt.end());
    return It->second;
  }

  [[nodiscard]] T operator[](Id Idx) const noexcept {
    assert(size_t(Idx) < FromInt.size());
    return FromInt[size_t(Idx)];
  }

  [[nodiscard]] size_t size() const noexcept { return FromInt.size(); }
  [[nodiscard]] size_t capacity() const noexcept {
    return FromInt.capacity() +
           ToInt.getMemorySize() / sizeof(typename decltype(ToInt)::value_type);
  }

  auto begin() const noexcept { return FromInt.begin(); }
  auto end() const noexcept { return FromInt.end(); }

private:
  llvm::DenseMap<T, Id> ToInt;
  llvm::SmallVector<T, 0> FromInt;
};

template <typename T, typename Id>
class Compressor<T, Id, std::enable_if_t<!CanEfficientlyPassByValue<T>>> {
public:
  void reserve(size_t Capacity) {
    assert(Capacity <= UINT32_MAX);
    ToInt.reserve(Capacity);
  }

  Id getOrInsert(const T &Elem) {
    if (auto It = ToInt.find(&Elem); It != ToInt.end()) {
      return It->second;
    }
    auto Ret = Id(FromInt.size());
    auto *Ins = &FromInt.emplace_back(Elem);
    ToInt[Ins] = Ret;
    return Ret;
  }

  Id getOrInsert(T &&Elem) {
    if (auto It = ToInt.find(&Elem); It != ToInt.end()) {
      return It->second;
    }
    auto Ret = Id(FromInt.size());
    auto *Ins = &FromInt.emplace_back(std::move(Elem));
    ToInt[Ins] = Ret;
    return Ret;
  }

  std::pair<Id, bool> insert(const T &Elem) {
    if (auto It = ToInt.find(&Elem); It != ToInt.end()) {
      return {It->second, false};
    }
    auto Ret = Id(FromInt.size());
    auto *Ins = &FromInt.emplace_back(Elem);
    ToInt[Ins] = Ret;
    return {Ret, true};
  }

  std::pair<Id, bool> insert(T &&Elem) {
    if (auto It = ToInt.find(&Elem); It != ToInt.end()) {
      return {It->second, false};
    }
    auto Ret = Id(FromInt.size());
    auto *Ins = &FromInt.emplace_back(std::move(Elem));
    ToInt[Ins] = Ret;
    return {Ret, true};
  }

  [[nodiscard]] std::optional<Id> getOrNull(const T &Elem) const {
    if (auto It = ToInt.find(&Elem); It != ToInt.end()) {
      return It->second;
    }
    return std::nullopt;
  }

  [[nodiscard]] Id get(const T &Elem) const {
    auto It = ToInt.find(&Elem);
    assert(It != ToInt.end());
    return It->second;
  }

  const T &operator[](Id Idx) const noexcept {
    assert(size_t(Idx) < FromInt.size());
    return FromInt[size_t(Idx)];
  }

  [[nodiscard]] size_t size() const noexcept { return FromInt.size(); }
  [[nodiscard]] size_t capacity() const noexcept {
    return FromInt.size() +
           ToInt.getMemorySize() / sizeof(typename decltype(ToInt)::value_type);
  }

  auto begin() const noexcept { return FromInt.begin(); }
  auto end() const noexcept { return FromInt.end(); }

private:
  struct DSI : llvm::DenseMapInfo<const T *> {
    static auto getHashValue(const T *Elem) noexcept {
      assert(Elem != nullptr);
      if constexpr (has_llvm_dense_map_info<T>) {
        return llvm::DenseMapInfo<T>::getHashValue(*Elem);
      } else {
        return std::hash<T>{}(*Elem);
      }
    }
    static auto isEqual(const T *LHS, const T *RHS) noexcept {
      if (LHS == RHS) {
        return true;
      }
      if (LHS == DSI::getEmptyKey() || LHS == DSI::getTombstoneKey() ||
          RHS == DSI::getEmptyKey() || RHS == DSI::getTombstoneKey()) {
        return false;
      }
      if constexpr (has_llvm_dense_map_info<T>) {
        return llvm::DenseMapInfo<T>::isEqual(*LHS, *RHS);
      } else {
        return *LHS == *RHS;
      }
    }
  };

  std::deque<T> FromInt;
  llvm::DenseMap<const T *, Id, DSI> ToInt;
};

struct NoneCompressor final {
  constexpr NoneCompressor() noexcept = default;

  template <typename T,
            typename = std::enable_if_t<!std::is_same_v<NoneCompressor, T>>>
  constexpr NoneCompressor(const T & /*unused*/) noexcept {}

  template <typename T>
  [[nodiscard]] decltype(auto) getOrInsert(T &&Val) const noexcept {
    return std::forward<T>(Val);
  }
  template <typename T>
  [[nodiscard]] decltype(auto) operator[](T &&Val) const noexcept {
    return std::forward<T>(Val);
  }
  void reserve(size_t /*unused*/) const noexcept {}

  [[nodiscard]] size_t size() const noexcept { return 0; }
  [[nodiscard]] size_t capacity() const noexcept { return 0; }
};

} // namespace psr
#endif
