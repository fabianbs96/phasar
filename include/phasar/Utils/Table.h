/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * Table.h
 *
 *  Created on: 07.11.2016
 *      Author: pdschbrt
 */

#ifndef PHASAR_UTILS_TABLE_H_
#define PHASAR_UTILS_TABLE_H_

#include "phasar/Utils/ByRef.h"
#include "phasar/Utils/DefaultValue.h"
#include "phasar/Utils/TypeTraits.h"

#include "llvm/Support/raw_ostream.h"

#include "parallel_hashmap/phmap_fwd_decl.h"

#include <cassert>
#include <concepts>
#include <mutex>
#include <optional>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

// we may wish to replace this by boost::multi_index at some point

namespace psr {

template <typename R, typename C, typename V> struct TableCell {
  constexpr TableCell() noexcept = default;
  constexpr TableCell(R Row, C Col, V Val) noexcept
      : Row(std::move(Row)), Column(std::move(Col)), Value(std::move(Val)) {}

  [[nodiscard]] constexpr ByConstRef<R> getRowKey() const noexcept {
    return Row;
  }
  [[nodiscard]] constexpr ByConstRef<C> getColumnKey() const noexcept {
    return Column;
  }
  [[nodiscard]] constexpr ByConstRef<V> getValue() const noexcept {
    return Value;
  }

  [[nodiscard]] constexpr friend bool operator<(const TableCell &Lhs,
                                                const TableCell &Rhs) noexcept {
    return std::tie(Lhs.Row, Lhs.Column, Lhs.Value) <
           std::tie(Rhs.Row, Rhs.Column, Rhs.Value);
  }
  [[nodiscard]] bool operator==(const TableCell &Rhs) const noexcept = default;

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                       const TableCell &Cell) {
    return OS << "Cell: " << Cell.Row << ", " << Cell.Column << ", "
              << Cell.Value;
  }

  R Row{};
  C Column{};
  V Value{};
};

template <typename R, typename C, typename V,
          template <typename, typename, typename...> class ContainerTy =
              std::unordered_map>
class Table {
  using Container = ContainerTy<R, ContainerTy<C, V>>;

public:
  using Cell = TableCell<R, C, V>;

  Table() noexcept = default;

  explicit Table(const Table &T) = default;
  Table &operator=(const Table &T) = delete;

  Table(Table &&T) noexcept = default;
  Table &operator=(Table &&T) noexcept = default;

  ~Table() = default;

  void insert(R Row, C Column, V Val) {
    // Associates the specified value with the specified keys.
    if constexpr (has_lazy_emplace<Container, R>) {
      auto &Inner = Tab.try_emplace_p(std::move(Row)).first->second;
      Inner.lazy_emplace(Column, [&](auto &&Ctor) {
        Ctor(std::move(Column), std::move(Val));
      });
    } else {
      Tab[std::move(Row)][std::move(Column)] = std::move(Val);
    }
  }

  void clear() noexcept { Tab.clear(); }

  [[nodiscard]] bool empty() const noexcept { return Tab.empty(); }

  [[nodiscard]] size_t size() const noexcept { return Tab.size(); }

  [[nodiscard]] size_t getApproxSizeInBytes() const noexcept {
    size_t Sz =
        (Tab.bucket_count() * sizeof(void *)) +
        (Tab.size() *
         sizeof(
             std::tuple<void *, void *, typename decltype(Tab)::value_type>));

    for (const auto &[RowKey, Row] : Tab) {
      Sz += (Row.bucket_count() * sizeof(void *)) +
            (Row.size() *
             sizeof(
                 std::tuple<void *, void *,
                            typename std::decay_t<decltype(Row)>::value_type>));
    }
    return Sz;
  }

  [[nodiscard]] std::set<Cell> cellSet() const {
    // Returns a set of all row key / column key / value triplets.
    std::set<Cell> Result;
    foreachCell([&](auto &&Row, auto &&Col, auto &&Val) {
      Result.emplace(Row, Col, Val);
    });
    return Result;
  }

  template <typename Fn> void foreachCell(Fn Handler) const {
    if constexpr (has_for_each<const Container>) {
      Tab.for_each([&](const auto &OuterEntry) {
        OuterEntry.second.for_each([&](const auto &InnerEntry) {
          std::invoke(Handler, OuterEntry.first, InnerEntry.first,
                      InnerEntry.second);
        });
      });
    } else {
      for (const auto &M1 : Tab) {
        for (const auto &M2 : M1.second) {
          std::invoke(Handler, M1.first, M2.first, M2.second);
        }
      }
    }
  }

  template <typename Fn> void foreachCell(Fn Handler) {
    if constexpr (has_for_each_m<Container>) {
      Tab.for_each_m([&](auto &OuterEntry) {
        OuterEntry.second.for_each_m([&](auto &InnerEntry) {
          std::invoke(Handler, OuterEntry.first, InnerEntry.first,
                      InnerEntry.second);
        });
      });
    } else {
      for (auto &M1 : Tab) {
        for (auto &M2 : M1.second) {
          std::invoke(Handler, M1.first, M2.first, M2.second);
        }
      }
    }
  }

  [[nodiscard]] std::vector<Cell> cellVec() const {
    // Returns a vector of all row key / column key / value triplets.
    std::vector<Cell> Result;

    Result.reserve(Tab.size()); // better than nothing...
    foreachCell([&](auto &&Row, auto &&Col, auto &&Val) {
      Result.emplace_back(Row, Col, Val);
    });

    return Result;
  }

  [[nodiscard]] Container column(ByConstRef<C> ColumnKey) const {
    // Returns a view of all mappings that have the given column key.
    std::unordered_map<R, V> Column;

    foreachCell([&](const auto &Row) {
      if (Row.second.count(ColumnKey)) {
        Column[Row.first] = Row.second[ColumnKey];
      }
    });

    return Column;
  }

  [[nodiscard]] bool contains(ByConstRef<R> RowKey,
                              ByConstRef<C> ColumnKey) const noexcept {
    // Returns true if the table contains a mapping with the specified row and
    // column keys.
    if constexpr (has_if_contains<Container, ByConstRef<R>>) {
      bool DoesContain = false;
      Tab.if_contains(RowKey, [&](const auto &Row) {
        DoesContain =
            Row.second.if_contains(ColumnKey, [&](const auto &Row) {});
      });
      return DoesContain;
    } else {
      if (auto RowIter = Tab.find(RowKey); RowIter != Tab.end()) {
        return RowIter->second.find(ColumnKey) != RowIter->second.end();
      }
      return false;
    }
  }

  [[nodiscard]] bool containsColumn(ByConstRef<C> ColumnKey) const noexcept {
    // Returns true if the table contains a mapping with the specified column.
    bool DoesContain = false;
    foreachCell([&](const auto &Entry) {
      if (Entry.second.count(ColumnKey)) {
        DoesContain = true;
        return;
      }
    });
    return DoesContain;
  }

  [[nodiscard]] bool containsRow(ByConstRef<R> RowKey) const noexcept {
    // Returns true if the table contains a mapping with the specified row
    // key.
    return Tab.count(RowKey);
  }

  [[nodiscard]] V &get(R RowKey, C ColumnKey) {
    // Returns the value corresponding to the given row and column keys, or
    // V() if no such mapping exists.
    return Tab[std::move(RowKey)][std::move(ColumnKey)];
  }

  void get(R RowKey, C ColumnKey, std::invocable<V &> auto Callback) {
    // Returns the value corresponding to the given row and column keys, or
    // V() if no such mapping exists.
    // TODO: below creates a data race. How to fix?
    auto &Inner = Tab.try_emplace_p(std::move(RowKey)).first->second;
    Inner.lazy_emplace_l(
        ColumnKey, [&](auto &Pair) { Callback(Pair.second); },
        [&](auto &&Ctor) {
          V TempV = V();
          Callback(TempV);
          Ctor(std::move(ColumnKey), std::move(TempV));
        });
  }

  [[nodiscard]] V getOrDefault(ByConstRef<R> RowKey,
                               ByConstRef<C> ColumnKey) const {
    if constexpr (has_if_contains<Container, ByConstRef<R>>) {
      V RetVal = V();

      Tab.if_contains(RowKey, [&](const auto &Entry) {
        Entry.second.if_contains(ColumnKey, [&](const auto &InnerEntry) {
          RetVal = InnerEntry.second;
        });
      });

      return RetVal;
    } else {
      auto OuterIt = Tab.find(RowKey);
      if (OuterIt == Tab.end()) {
        return V();
      }
      auto InnerIt = OuterIt->second.find(ColumnKey);
      if (InnerIt == OuterIt->second.end()) {
        return V();
      }

      return InnerIt->second;
    }
  }

  [[nodiscard]] std::optional<V> tryGet(ByConstRef<R> RowKey,
                                        ByConstRef<C> ColumnKey) {
    if constexpr (has_if_contains<Container, ByConstRef<R>>) {
      std::optional<V> RetVal = std::nullopt;

      Tab.if_contains(RowKey, [&](auto &Entry) {
        Entry.second.if_contains(
            ColumnKey, [&](auto &InnerEntry) { RetVal = InnerEntry.second; });
      });

      return RetVal;
    } else {
      auto OuterIt = Tab.find(RowKey);
      if (OuterIt == Tab.end()) {
        return std::nullopt;
      }
      auto InnerIt = OuterIt->second.find(ColumnKey);
      if (InnerIt == OuterIt->second.end()) {
        return std::nullopt;
      }

      return InnerIt->second;
    }
  }

  [[nodiscard]] ByConstRef<V> get(ByConstRef<R> RowKey,
                                  ByConstRef<C> ColumnKey) const noexcept {
    // Returns the value corresponding to the given row and column keys, or
    // V() if no such mapping exists.
    if constexpr (has_if_contains<Container, ByConstRef<R>>) {
      ByConstRef<V> RetVal = getDefaultValue<V>();

      Tab.if_contains(RowKey, [&](auto &Entry) {
        Entry.second.if_contains(
            ColumnKey, [&](auto &InnerEntry) { RetVal = InnerEntry.second; });
      });

      return RetVal;
    } else {
      auto OuterIt = Tab.find(RowKey);
      if (OuterIt == Tab.end()) {
        return getDefaultValue<V>();
      }

      auto It = OuterIt->second.find(ColumnKey);
      if (It == OuterIt->second.end()) {
        return getDefaultValue<V>();
      }

      return It->second;
    }
  }

  V remove(ByConstRef<R> RowKey, ByConstRef<C> ColumnKey) {
    // Removes the mapping, if any, associated with the given keys.
    if constexpr (has_if_contains<Container, ByConstRef<R>>) {
      V RetVal = V();

      // TODO: ask Fabian if this logic is sound
      Tab.if_contains(RowKey, [&](auto &Entry) {
        Entry.second.erase_if(
            ColumnKey, [&](auto &InnerEntry) { RetVal = InnerEntry.second; });
      });

      return RetVal;
    } else {
      auto OuterIt = Tab.find(RowKey);
      if (OuterIt == Tab.end()) {
        return V();
      }

      auto It = OuterIt->second.find(ColumnKey);
      if (It == OuterIt->second.end()) {
        return V();
      }

      auto Ret = std::move(It->second);

      OuterIt->second.erase(It);
      if (OuterIt->second.empty()) {
        Tab.erase(OuterIt);
      }

      return Ret;
    }
  }

  void remove(ByConstRef<R> RowKey) { Tab.erase(RowKey); }

  [[nodiscard]] ContainerTy<C, V> &row(R RowKey) {
    // Returns a view of all mappings that have the given row key.
    // TODO: can this be made thread safe, given that it returns a reference?
    // TODO: do we even need this to be made thread safe? Or can we just not use
    // it and use other functions if we need thread safety?
    return Tab[RowKey];
  }

  [[nodiscard]] ByConstRef<ContainerTy<C, V>>
  row(ByConstRef<R> RowKey) const noexcept {
    // Returns a view of all mappings that have the given row key.
    if constexpr (has_if_contains<Container, ByConstRef<R>>) {
      ByConstRef<Container> RetVal;
      Tab.if_contains(RowKey,
                      [&](const auto &Entry) { RetVal = Entry.second; });
      return RetVal;
    } else {
      auto It = Tab.find(RowKey);
      if (It == Tab.end()) {
        return getDefaultValue<std::unordered_map<C, V>>();
      }
      return It->second;
    }
  }

  [[nodiscard]] const Container &rowMap() const & noexcept {
    // Returns a view that associates each row key with the corresponding map
    // from column keys to values.
    return Tab;
  }
  [[nodiscard]] Container &&rowMap() && noexcept {
    // Returns a view that associates each row key with the corresponding map
    // from column keys to values.
    return std::move(Tab);
  }
  [[nodiscard]] const Container &rowMapView() const noexcept {
    // Returns a view that associates each row key with the corresponding map
    // from column keys to values.
    return Tab;
  }

  void reserve(size_t Capacity) { Tab.reserve(Capacity); }

  bool operator==(const Table<R, C, V, ContainerTy> &Other) noexcept {
    return Tab == Other.Tab;
  }

  bool operator<(const Table<R, C, V, ContainerTy> &Other) noexcept {
    return Tab < Other.Tab;
  }

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                       const Table<R, C, V, ContainerTy> &Tab) {
    Tab.foreachCell([&](const auto &M1, const auto &M2) {
      OS << "< " << M1.first << " , " << M2.first << " , " << M2.second
         << " >\n";
    });
    return OS;
  }

private:
  // std::unordered_map<R, std::unordered_map<C, V>> Tab{};
  Container Tab{};
};

} // namespace psr

#endif
