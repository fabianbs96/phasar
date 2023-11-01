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

#include "llvm/Support/raw_ostream.h"

#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

// we may wish to replace this by boost::multi_index at some point

namespace psr {

template <typename R, typename C, typename V> class Table {
public:
  struct Cell {
    Cell() noexcept = default;
    Cell(R Row, C Col, V Val) noexcept
        : Row(std::move(Row)), Column(std::move(Col)), Value(std::move(Val)) {}

    [[nodiscard]] ByConstRef<R> getRowKey() const noexcept { return Row; }
    [[nodiscard]] ByConstRef<C> getColumnKey() const noexcept { return Column; }
    [[nodiscard]] ByConstRef<V> getValue() const noexcept { return Value; }

    friend llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                         const Cell &Cell) {
      return OS << "Cell: " << Cell.r << ", " << Cell.c << ", " << Cell.v;
    }
    friend bool operator<(const Cell &Lhs, const Cell &Rhs) noexcept {
      return std::tie(Lhs.Row, Lhs.Column, Lhs.Value) <
             std::tie(Rhs.Row, Rhs.Column, Rhs.Value);
    }
    friend bool operator==(const Cell &Lhs, const Cell &Rhs) noexcept {
      return std::tie(Lhs.Row, Lhs.Column, Lhs.Value) ==
             std::tie(Rhs.Row, Rhs.Column, Rhs.Value);
    }

    R Row{};
    C Column{};
    V Value{};
  };

  Table() noexcept = default;

  explicit Table(const Table &T) = default;
  Table &operator=(const Table &T) = delete;

  Table(Table &&T) noexcept = default;
  Table &operator=(Table &&T) noexcept = default;

  ~Table() = default;

  void insert(R Row, C Column, V Val) {
    // Associates the specified value with the specified keys.
    Tab[std::move(Row)][std::move(Column)] = std::move(Val);
  }

  void clear() noexcept { Tab.clear(); }

  [[nodiscard]] bool empty() const noexcept { return Tab.empty(); }

  [[nodiscard]] size_t size() const noexcept { return Tab.size(); }

  [[nodiscard]] std::set<Cell> cellSet() const {
    // Returns a set of all row key / column key / value triplets.
    std::set<Cell> Result;
    for (const auto &M1 : Tab) {
      for (const auto &M2 : M1.second) {
        Result.emplace(M1.first, M2.first, M2.second);
      }
    }
    return Result;
  }

  template <typename Fn> void foreachCell(Fn Handler) const {
    for (const auto &M1 : Tab) {
      for (const auto &M2 : M1.second) {
        std::invoke(Handler, M1.first, M2.first, M2.second);
      }
    }
  }
  template <typename Fn> void foreachCell(Fn Handler) {
    for (auto &M1 : Tab) {
      for (auto &M2 : M1.second) {
        std::invoke(Handler, M1.first, M2.first, M2.second);
      }
    }
  }

  [[nodiscard]] std::vector<Cell> cellVec() const {
    // Returns a vector of all row key / column key / value triplets.
    std::vector<Cell> Result;
    Result.reserve(Tab.size()); // better than nothing...
    for (const auto &M1 : Tab) {
      for (const auto &M2 : M1.second) {
        Result.emplace_back(M1.first, M2.first, M2.second);
      }
    }
    return Result;
  }

  [[nodiscard]] std::unordered_map<R, V> column(ByConstRef<C> ColumnKey) const {
    // Returns a view of all mappings that have the given column key.
    std::unordered_map<R, V> Column;
    for (const auto &Row : Tab) {
      if (Row.second.count(ColumnKey)) {
        Column[Row.first] = Row.second[ColumnKey];
      }
    }
    return Column;
  }

  [[nodiscard]] bool contains(ByConstRef<R> RowKey,
                              ByConstRef<C> ColumnKey) const noexcept {
    // Returns true if the table contains a mapping with the specified row and
    // column keys.
    if (auto RowIter = Tab.find(RowKey); RowIter != Tab.end()) {
      return RowIter->second.find(ColumnKey) != RowIter->second.end();
    }
    return false;
  }

  [[nodiscard]] bool containsColumn(ByConstRef<C> ColumnKey) const noexcept {
    // Returns true if the table contains a mapping with the specified column.
    for (const auto &M1 : Tab) {
      if (M1.second.count(ColumnKey)) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] bool containsRow(ByConstRef<R> RowKey) const noexcept {
    // Returns true if the table contains a mapping with the specified row key.
    return Tab.count(RowKey);
  }

  V DefaultV = V();

  [[nodiscard]] V &get(R RowKey, C ColumnKey) {
    // Returns the value corresponding to the given row and column keys, or V()
    // if no such mapping exists.

    // TODO: ask fabian about this function. The comment above suggests that if
    // no mapping exists, that case is being caught. However, it isn't, which
    // leads to a seg fault
    // The code below that catches that case was written by me. Also, the
    // comment suggests that in that case, a V() will be returned, which makes
    // no sense since the return type is of V&
    // UPDATE: okay since a V& is expected to be returned, I created a variable
    // for this, so an address exists that can be referenced. Technically we
    // could also set the return type to just V, but that would probably be a
    // lot slower.
    // UPDATE: wait, if I create a V() and return the reference to this, that
    // could actually be a problem. If this is returned at least twice and
    // someone treats them as seperate objects, they might get annoying bugs.
    // But maybe they are in the fault then, given that they didn't check the
    // return value correctly?

    llvm::outs() << "\n\n Tab.find(RowKey) ==  \n\n";
    llvm::outs().flush();
    if (Tab.find(RowKey) == Tab.end()) {
      return DefaultV;
    }

    llvm::outs() << "\n\n Tab[RowKey].find(ColumnKey) ==  \n\n";
    llvm::outs().flush();
    if (Tab[RowKey].find(ColumnKey) == Tab[RowKey].end()) {
      return DefaultV;
    }

    llvm::outs() << "\n\n Tab[std::move(RowKey)][std::move(ColumnKey)] =  \n\n";
    llvm::outs().flush();
    return Tab[std::move(RowKey)][std::move(ColumnKey)];
  }

  [[nodiscard]] ByConstRef<V> get(ByConstRef<R> RowKey,
                                  ByConstRef<C> ColumnKey) const noexcept {
    // Returns the value corresponding to the given row and column keys, or V()
    // if no such mapping exists.
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

  V remove(ByConstRef<R> RowKey, ByConstRef<C> ColumnKey) {
    // Removes the mapping, if any, associated with the given keys.

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

  void remove(ByConstRef<R> RowKey) { Tab.erase(RowKey); }

  [[nodiscard]] std::unordered_map<C, V> &row(R RowKey) {
    // Returns a view of all mappings that have the given row key.
    return Tab[RowKey];
  }

  [[nodiscard]] ByConstRef<std::unordered_map<C, V>>
  row(ByConstRef<R> RowKey) const noexcept {
    // Returns a view of all mappings that have the given row key.
    auto It = Tab.find(RowKey);
    if (It == Tab.end()) {
      return getDefaultValue<std::unordered_map<C, V>>();
    }
    return It->second;
  }

  [[nodiscard]] const std::unordered_map<R, std::unordered_map<C, V>> &
  rowMap() const noexcept {
    // Returns a view that associates each row key with the corresponding map
    // from column keys to values.
    return Tab;
  }

  bool operator==(const Table<R, C, V> &Other) noexcept {
    return Tab == Other.Tab;
  }

  bool operator<(const Table<R, C, V> &Other) noexcept {
    return Tab < Other.Tab;
  }

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                       const Table<R, C, V> &Tab) {
    for (const auto &M1 : Tab.Tab) {
      for (const auto &M2 : M1.second) {
        OS << "< " << M1.first << " , " << M2.first << " , " << M2.second
           << " >\n";
      }
    }
    return OS;
  }

private:
  std::unordered_map<R, std::unordered_map<C, V>> Tab{};
};

} // namespace psr

#endif
