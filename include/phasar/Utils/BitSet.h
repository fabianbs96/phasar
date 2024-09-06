/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#ifndef PHASAR_UTILS_BITSET_H
#define PHASAR_UTILS_BITSET_H

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallBitVector.h"

#include <cstddef>
#include <cstdint>
#include <iterator>

namespace psr {
template <typename IdT> class BitSet {
public:
  class Iterator {
  public:
    using value_type = IdT;
    using reference = IdT;
    using pointer = const IdT *;
    using difference_type = ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    Iterator(llvm::SmallBitVector::const_set_bits_iterator It) noexcept
        : It(It) {}

    Iterator &operator++() noexcept {
      ++It;
      return *this;
    }
    Iterator operator++(int) noexcept {
      auto Ret = *this;
      ++*this;
      return Ret;
    }
    reference operator*() const noexcept { return IdT(*It); }

    bool operator==(const Iterator &Other) const noexcept {
      return It == Other.It;
    }
    bool operator!=(const Iterator &Other) const noexcept {
      return !(*this == Other);
    }

  private:
    llvm::SmallBitVector::const_set_bits_iterator It;
  };

  using iterator = Iterator;
  using value_type = IdT;

  BitSet() noexcept = default;
  explicit BitSet(size_t InitialCapacity) : Bits(InitialCapacity) {}
  explicit BitSet(size_t InitialCapacity, bool InitialValue)
      : Bits(InitialCapacity, InitialValue) {}

  void reserve(size_t Cap) {
    if (Bits.size() < Cap) {
      Bits.resize(Cap);
    }
  }

  [[nodiscard]] bool contains(IdT Id) const noexcept {
    auto Index = uint32_t(Id);
    return Bits.size() > Index && Bits.test(Index);
  }

  void insert(IdT Id) {
    auto Index = uint32_t(Id);
    if (Bits.size() <= Index) {
      Bits.resize(Index + 1);
    }

    Bits.set(Index);
  }

  [[nodiscard]] bool tryInsert(IdT Id) {
    auto Index = uint32_t(Id);
    if (Bits.size() <= Index) {
      Bits.resize(Index + 1);
    }

    bool Ret = !Bits.test(Index);
    Bits.set(Index);
    return Ret;
  }

  void erase(IdT Id) noexcept {
    if (Bits.size() > size_t(Id)) {
      Bits.reset(uint32_t(Id));
    }
  }
  [[nodiscard]] bool tryErase(IdT Id) noexcept {
    if (contains(Id)) {
      return Bits.reset(uint32_t(Id)), true;
    }

    return false;
  }

  void mergeWith(const BitSet &Other) { Bits |= Other.Bits; }

  bool tryMergeWith(const BitSet &Other) {
    /// TODO: Make this more efficient
    return *this == Other ? false : (mergeWith(Other), true);
  }

  void clear() noexcept { Bits.reset(); }

  [[nodiscard]] friend bool operator==(const BitSet &Lhs,
                                       const BitSet &Rhs) noexcept {
    bool LeftEmpty = Lhs.Bits.none();
    bool RightEmpty = Rhs.Bits.none();
    if (LeftEmpty || RightEmpty) {
      return LeftEmpty == RightEmpty;
    }
    // Check, whether Lhs and Rhs actually have the same bits set and not
    // whether their internal representation is exactly identitcal
    uintptr_t LhsStore{};
    uintptr_t RhsStore{};

    auto LhsWords = Lhs.Bits.getData(LhsStore);
    auto RhsWords = Rhs.Bits.getData(RhsStore);
    if (LhsWords.size() == RhsWords.size()) {
      return LhsWords == RhsWords;
    }
    auto MinSize = std::min(LhsWords.size(), RhsWords.size());
    if (LhsWords.slice(0, MinSize) != RhsWords.slice(0, MinSize)) {
      return false;
    }
    auto Rest = (LhsWords.size() > RhsWords.size() ? LhsWords : RhsWords)
                    .slice(MinSize);
    return std::all_of(Rest.begin(), Rest.end(),
                       [](auto Word) { return Word == 0; });
  }

  [[nodiscard]] friend bool operator!=(const BitSet &Lhs,
                                       const BitSet &Rhs) noexcept {
    return !(Lhs == Rhs);
  }

  [[nodiscard]] bool any() const noexcept { return Bits.any(); }

  [[nodiscard]] iterator begin() const noexcept {
    return Bits.set_bits_begin();
  }
  [[nodiscard]] iterator end() const noexcept { return Bits.set_bits_end(); }

  void operator|=(const BitSet &Other) { Bits |= Other.Bits; }
  void operator-=(const BitSet &Other) { Bits.reset(Other.Bits); }

  [[nodiscard]] BitSet operator-(const BitSet &Other) const {
    // TODO: keep allocation small by looping from the end and truncating all
    // words that result in all-zero
    auto Ret = *this;
    Ret -= Other;
    return Ret;
  }

  BitSet &insertAllOf(const BitSet &Other) {
    Bits |= Other.Bits;
    return *this;
  }
  BitSet &eraseAllOf(const BitSet &Other) {
    Bits.reset(Other.Bits);
    return *this;
  }

  [[nodiscard]] bool isSubsetOf(const BitSet &Of) const {
    uintptr_t Buf = 0;
    uintptr_t OfBuf = 0;

    auto Words = Bits.getData(Buf);
    auto OfWords = Of.Bits.getData(OfBuf);
    if (Words.size() > OfWords.size()) {
      if (llvm::any_of(Words.drop_front(OfWords.size()),
                       [](uintptr_t W) { return W != 0; })) {
        return false;
      }
    }

    for (auto [W, OfW] : llvm::zip(Words, OfWords)) {
      if ((W & ~OfW) != 0) {
        return false;
      }
    }

    return true;
  }

  [[nodiscard]] bool isSupersetOf(const BitSet &Of) const {
    return Of.isSubsetOf(*this);
  }

  // The number of bits available
  [[nodiscard]] size_t capacity() const noexcept { return Bits.size(); }
  // The number of bits set to 1
  [[nodiscard]] size_t size() const noexcept { return Bits.count(); }
  [[nodiscard]] bool empty() const noexcept { return Bits.none(); }

  [[nodiscard]] bool test(uint32_t Ident) { return Bits.test(Ident); }

private:
  llvm::SmallBitVector Bits;
};
} // namespace psr

#endif
