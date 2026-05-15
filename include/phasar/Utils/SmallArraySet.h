/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_UTILS_SMALLARRAYSET_H
#define PHASAR_UTILS_SMALLARRAYSET_H

#include "phasar/Utils/ByRef.h"
#include "phasar/Utils/Macros.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

#include <algorithm>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <type_traits>

namespace psr {

template <typename T,
          unsigned N =
              llvm::CalculateSmallVectorDefaultInlinedElements<T>::value>
class SmallArraySet {
public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using iterator = typename llvm::SmallVector<T, N>::iterator;
  using const_iterator = typename llvm::SmallVector<T, N>::const_iterator;
  using value_type = T;
  using reference = T &;
  using const_reference = const T &;
  using pointer = T *;
  using const_pointer = const T *;

  SmallArraySet() noexcept = default;

  SmallArraySet(std::initializer_list<T> IList)
      : Dirty(!std::ranges::is_sorted(IList)) {
    Arr.append(IList.begin(), IList.end());
  }

  void reserve(size_t NumElems) { Arr.reserve(NumElems); }

  template <typename TT = T>
  void insert(TT &&Elem)
    requires(std::is_constructible_v<T, TT>)
  {
    if (!empty()) {
      if (Elem == Arr.back()) {
        return;
      }
      Dirty |= Elem < Arr.back();
    }

    Arr.emplace_back(PSR_FWD(Elem));
  }

  template <typename IterT> auto insert(IterT From, IterT To) {
    if (From == To) {
      return;
    }
    Arr.append(From, To);
    Dirty |= !(empty() || *From >= Arr.back()) || !std::is_sorted(From, To);
  }

  [[nodiscard]] iterator begin() noexcept {
    deduplicate();
    return Arr.begin();
  }
  [[nodiscard]] iterator end() noexcept {
    deduplicate();
    return Arr.end();
  }

  [[nodiscard]] const_iterator begin() const noexcept {
    deduplicate();
    return Arr.begin();
  }
  [[nodiscard]] const_iterator end() const noexcept {
    deduplicate();
    return Arr.end();
  }

  [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
  [[nodiscard]] const_iterator cend() const noexcept { return begin(); }

  [[nodiscard]] bool empty() const noexcept { return Arr.empty(); }
  [[nodiscard]] size_t size() const noexcept {
    deduplicate();
    return Arr.size();
  }

  [[nodiscard]] int count(ByConstRef<T> Elem) const noexcept {
    return contains(Elem) ? 1 : 0;
  }
  [[nodiscard]] bool contains(ByConstRef<T> Elem) const noexcept {
    deduplicate();
    return std::ranges::binary_search(Arr, Elem);
  }

  void deduplicate() const {
    if (!Dirty) {
      return;
    }
    std::ranges::sort(Arr);
    Arr.erase(std::ranges::unique(Arr).begin(), Arr.end());
    Dirty = false;
  }

  bool operator<(const SmallArraySet<T, N> &Other) const {
    deduplicate();
    return Arr < Other.Arr;
  }

private:
  // TODO: We should probably avoid 'mutable'
  mutable llvm::SmallVector<T, N> Arr;
  mutable bool Dirty = false;
};

} // namespace psr

#endif // PHASAR_UTILS_SMALLARRAYSET_H
