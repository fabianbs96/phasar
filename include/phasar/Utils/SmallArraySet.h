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
#include <initializer_list>
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

  template <typename TT = T,
            typename = std::enable_if_t<std::is_constructible_v<T, const TT &>>>
  SmallArraySet(std::initializer_list<TT> IList) {
    Arr.append(IList.begin(), IList.end());
  }

  llvm::SmallVector<T, N> Arr;

  void reserve(size_t NumElems) { Arr.reserve(NumElems); }

  template <typename TT = T>
  std::enable_if_t<std::is_constructible_v<T, TT>> insert(TT &&Elem) {
    Arr.emplace_back(PSR_FWD(Elem));
  }

  template <typename IterT>
  auto insert(IterT From, IterT To)
      -> decltype(this->Arr.append(std::move(From), std::move(To))) {
    Arr.append(std::move(From), std::move(To));
  }

  [[nodiscard]] iterator begin() noexcept { return Arr.begin(); }
  [[nodiscard]] iterator end() noexcept { return Arr.end(); }

  [[nodiscard]] const_iterator begin() const noexcept { return Arr.begin(); }
  [[nodiscard]] const_iterator end() const noexcept { return Arr.end(); }

  [[nodiscard]] const_iterator cbegin() const noexcept { return Arr.begin(); }
  [[nodiscard]] const_iterator cend() const noexcept { return Arr.end(); }

  [[nodiscard]] bool empty() const noexcept { return Arr.empty(); }
  [[nodiscard]] size_t size() const noexcept { return Arr.size(); }

  [[nodiscard]] int count(ByConstRef<T> Elem) const noexcept {
    return llvm::count(Arr, Elem);
  }
  [[nodiscard]] bool contains(ByConstRef<T> Elem) const noexcept {
    return llvm::is_contained(Arr, Elem);
  }

  void deduplicate() {
    std::sort(Arr.begin(), Arr.end());
    Arr.erase(std::unique(Arr.begin(), Arr.end()), Arr.end());
  }

  bool operator<(const SmallArraySet<T, N> &Other) const {
    return Arr < Other.Arr;
  }
};

} // namespace psr

#endif // PHASAR_UTILS_SMALLARRAYSET_H
