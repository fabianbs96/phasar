/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * JumpFunctions.h
 *
 *  Created on: 17.08.2016
 *      Author: pdschbrt
 */

#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_JUMPFUNCTIONS_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_JUMPFUNCTIONS_H

#include "phasar/DataFlow/IfdsIde/EdgeFunctionUtils.h"
#include "phasar/DataFlow/IfdsIde/IfdsIdeDomain.h"
#include "phasar/Utils/ByRef.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/Table.h"
#include "phasar/Utils/TypeTraits.h"

#include "llvm/ADT/SmallVector.h"

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <utility>

namespace psr {

template <typename AnalysisDomainTy, typename Container,
          template <typename, typename, typename...> class MapContainerTy =
              std::unordered_map>
class JumpFunctions {
public:
  using l_t = typename AnalysisDomainTy::l_t;
  using d_t = typename AnalysisDomainTy::d_t;
  using n_t = typename AnalysisDomainTy::n_t;

protected:
  // mapping from target node and value to a list of all source values and
  // associated functions where the list is implemented as a mapping from
  // the source value to the function we exclude empty default functions
  Table<n_t, d_t, llvm::SmallVector<std::pair<d_t, EdgeFunction<l_t>>, 1>,
        MapContainerTy>
      NonEmptyReverseLookup;
  // mapping from source value and target node to a list of all target values
  // and associated functions where the list is implemented as a mapping from
  // the source value to the function we exclude empty default functions
  Table<d_t, n_t, llvm::SmallVector<std::pair<d_t, EdgeFunction<l_t>>, 1>,
        MapContainerTy>
      NonEmptyForwardLookup;
  // a mapping from target node to a list of triples consisting of source value,
  // target value and associated function; the triple is implemented by a table
  // we exclude empty default functions
  MapContainerTy<n_t, Table<d_t, d_t, EdgeFunction<l_t>, MapContainerTy>>
      NonEmptyLookupByTargetNode;

  std::mutex NonEmptyReverseLookupMutex;
  std::mutex NonEmptyForwardLookupMutex;
  std::mutex NonEmptyLookupByTargetNodeMutex;

public:
  JumpFunctions() noexcept = default;
  ~JumpFunctions() = default;

  JumpFunctions(const JumpFunctions &JFs) = default;
  JumpFunctions &operator=(const JumpFunctions &JFs) = default;
  JumpFunctions(JumpFunctions &&JFs) noexcept = default;
  JumpFunctions &operator=(JumpFunctions &&JFs) noexcept = default;

  /**
   * Records a jump function. The source statement is implicit.
   * @see PathEdge
   */
  void addFunction(d_t SourceVal, n_t Target, d_t TargetVal,
                   EdgeFunction<l_t> EdgeFunc) {
    PHASAR_LOG_LEVEL(DEBUG, "Start adding new jump function");
    PHASAR_LOG_LEVEL(DEBUG, "Fact at source : " << DToString(SourceVal));
    PHASAR_LOG_LEVEL(DEBUG, "Fact at target : " << DToString(TargetVal));
    PHASAR_LOG_LEVEL(DEBUG, "Destination    : " << NToString(Target));
    PHASAR_LOG_LEVEL(DEBUG, "Edge Function  : " << EdgeFunc);
    // we do not store the default function (all-top)
    if (llvm::isa<AllTop<l_t>>(EdgeFunc)) {
      return;
    }

    NonEmptyReverseLookup.get(Target, TargetVal, [&](auto &SourceValToFunc) {
      if (auto Find = std::find_if(
              SourceValToFunc.begin(), SourceValToFunc.end(),
              [SourceVal](const std::pair<d_t, EdgeFunction<l_t>> &Entry) {
                return SourceVal == Entry.first;
              });
          Find != SourceValToFunc.end()) {
        // it is important that existing values in JumpFunctions
        // are overwritten
        Find->second = EdgeFunc;
      } else {
        SourceValToFunc.emplace_back(SourceVal, EdgeFunc);
      }
    });

    NonEmptyForwardLookup.get(SourceVal, Target, [&](auto &TargetValToFunc) {
      if (auto Find = std::find_if(
              TargetValToFunc.begin(), TargetValToFunc.end(),
              [TargetVal](const std::pair<d_t, EdgeFunction<l_t>> &Entry) {
                return TargetVal == Entry.first;
              });
          Find != TargetValToFunc.end()) {
        // it is important that existing values in JumpFunctions
        // are overwritten
        Find->second = EdgeFunc;
      } else {
        TargetValToFunc.emplace_back(TargetVal, EdgeFunc);
      }
    });

    if constexpr (has_try_emplace_p<const Container, n_t>) {
      // V Table::insert(R r, C c, V v) always overrides (see
      // comments above)
      auto &Inner =
          NonEmptyLookupByTargetNode.try_emplace_p(Target).first->second;
      Inner.insert(SourceVal, TargetVal, EdgeFunc);
    } else {
      NonEmptyLookupByTargetNode[Target].insert(SourceVal, TargetVal, EdgeFunc);
    }

    PHASAR_LOG_LEVEL(DEBUG, "End adding new jump function");
  }

  /**
   * Atomically looks up the jump function currently stored for
   * (SourceVal, Target, TargetVal) (defaulting to TopFunction if none
   * exists yet), combines it with EdgeFunc via Combine(Old, New), and -- if
   * the result differs from the previous value -- stores it.
   *
   * The read-combine-conditionally-write sequence, including keeping the
   * forward-lookup and by-target-node indices in sync, happens while
   * holding the (Target, TargetVal) bucket lock of the underlying table.
   * This makes the whole operation atomic with respect to other concurrent
   * calls for the same (SourceVal, Target, TargetVal) triple -- avoiding
   * lost updates when several threads propagate into the same jump
   * function concurrently -- without requiring any solver-wide lock:
   * concurrent updates for different (Target, TargetVal) keys can still
   * proceed fully in parallel.
   *
   * @return the combined edge function together with a flag indicating
   * whether it differs from the previously stored one.
   */
  template <typename CombineFn>
  std::pair<EdgeFunction<l_t>, bool>
  combineAndAddFunction(d_t SourceVal, n_t Target, d_t TargetVal,
                        EdgeFunction<l_t> EdgeFunc,
                        EdgeFunction<l_t> TopFunction, CombineFn Combine) {
    EdgeFunction<l_t> FPrime = TopFunction;
    bool IsNew = false;

    NonEmptyReverseLookup.get(Target, TargetVal, [&](auto &SourceValToFunc) {
      auto Find = std::find_if(
          SourceValToFunc.begin(), SourceValToFunc.end(),
          [SourceVal](const std::pair<d_t, EdgeFunction<l_t>> &Entry) {
            return SourceVal == Entry.first;
          });
      EdgeFunction<l_t> Old =
          Find != SourceValToFunc.end() ? Find->second : TopFunction;

      FPrime = Combine(Old, EdgeFunc);
      IsNew = FPrime != Old;

      // we do not store the default function (all-top)
      if (!IsNew || llvm::isa<AllTop<l_t>>(FPrime)) {
        return;
      }

      if (Find != SourceValToFunc.end()) {
        // it is important that existing values in JumpFunctions
        // are overwritten
        Find->second = FPrime;
      } else {
        SourceValToFunc.emplace_back(SourceVal, FPrime);
      }

      NonEmptyForwardLookup.get(SourceVal, Target, [&](auto &TargetValToFunc) {
        auto FwdFind = std::find_if(
            TargetValToFunc.begin(), TargetValToFunc.end(),
            [TargetVal](const std::pair<d_t, EdgeFunction<l_t>> &Entry) {
              return TargetVal == Entry.first;
            });
        if (FwdFind != TargetValToFunc.end()) {
          FwdFind->second = FPrime;
        } else {
          TargetValToFunc.emplace_back(TargetVal, FPrime);
        }
      });

      // V Table::insert(R r, C c, V v) always overrides (see comments
      // above)
      auto &Inner =
          NonEmptyLookupByTargetNode.try_emplace_p(Target).first->second;
      Inner.insert(SourceVal, TargetVal, FPrime);
    });

    return {FPrime, IsNew};
  }

  /**
   * Returns, for a given target statement and value all associated
   * source values, and for each the associated edge function.
   * The return value is a mapping from source value to function.
   */
  void reverseLookup(n_t Target, d_t TargetVal,
                     std::invocable<llvm::SmallVectorImpl<
                         std::pair<d_t, EdgeFunction<l_t>>> &> auto Callback) {
    if (!NonEmptyReverseLookup.contains(Target, TargetVal)) {
      return;
    }

    NonEmptyReverseLookup.get(Target, TargetVal, std::move(Callback));
  }

  /**
   * Returns, for a given source value and target statement all
   * associated target values, and for each the associated edge function.
   * The return value is a mapping from target value to function.
   */
  void forwardLookup(d_t SourceVal, n_t Target,
                     std::invocable<llvm::SmallVectorImpl<
                         std::pair<d_t, EdgeFunction<l_t>>> &> auto Callback) {
    if (!NonEmptyForwardLookup.contains(SourceVal, Target)) {
      return;
    }

    NonEmptyForwardLookup.get(SourceVal, Target, std::move(Callback));
  }

  /**
   * Returns for a given target statement all jump function records with this
   * target.
   * The return value is a set of records of the form
   * (sourceVal,targetVal,edgeFunction).
   */
  Table<d_t, d_t, EdgeFunction<l_t>, MapContainerTy> &
  lookupByTarget(n_t Target) {
    return NonEmptyLookupByTargetNode[Target];
  }

  template <typename HandlerFn>
  void foreachEdgeFunction(HandlerFn Handler) const {
    if constexpr (has_for_each<const Container>) {
      std::lock_guard Guard(NonEmptyForwardLookupMutex);
      foreachEdgeFunction(Handler);
    } else {
      foreachEdgeFunction(Handler);
    }
  }

  /**
   * Removes a jump function. The source statement is implicit.
   * @see PathEdge
   * @return True if the function has actually been removed. False if it was not
   * there anyway.
   */
  bool removeFunction(d_t SourceVal, n_t Target, d_t TargetVal) {
    NonEmptyReverseLookup.get(Target, TargetVal, [&](auto &SourceValToFunc) {
      if (auto Find = std::find_if(
              SourceValToFunc.begin(), SourceValToFunc.end(),
              [SourceVal](const std::pair<d_t, EdgeFunction<l_t>> &Entry) {
                return SourceVal == Entry.first;
              });
          Find != SourceValToFunc.end()) {
        SourceValToFunc.erase(Find);
      }
    });

    auto &TargetValToFunc = NonEmptyForwardLookup.get(
        SourceVal, Target, [&](auto &TargetValToFunc) {
          if (auto Find = std::find_if(
                  TargetValToFunc.begin(), TargetValToFunc.end(),
                  [TargetVal](const std::pair<d_t, EdgeFunction<l_t>> &Entry) {
                    return TargetVal == Entry.first;
                  });
              Find != TargetValToFunc.end()) {
            TargetValToFunc.erase(Find);
          }
        });

    if constexpr (has_for_each<const Container>) {
      std::lock_guard LookupGuard(NonEmptyLookupByTargetNodeMutex);
      return NonEmptyLookupByTargetNode.erase(Target);
    } else {
      return NonEmptyLookupByTargetNode.erase(Target);
    }
  }

private:
  template <typename HandlerFn>
  void foreachEdgeFunctionImpl(HandlerFn Handler) const {
    NonEmptyForwardLookup.foreachCell(
        [Handler = std::move(Handler)](ByConstRef<d_t> /*Row*/,
                                       ByConstRef<n_t> /*Col*/,
                                       const auto &TargetFactAndEF) {
          for (const auto &[TargetFact, EF] : TargetFactAndEF) {
            std::invoke(Handler, EF);
          }
        });
  }

  void clearImpl() {
    NonEmptyReverseLookup.clear();
    NonEmptyForwardLookup.clear();
    NonEmptyLookupByTargetNode.clear();
  }

  void printJumpFunctionsImpl(llvm::raw_ostream &OS) {
    OS << "\n******************************************************";
    OS << "\n*              Print all Jump Functions              *";
    OS << "\n******************************************************\n";

    for (auto &Entry : NonEmptyLookupByTargetNode) {
      std::string NLabel = NToString(Entry.first);
      OS << "\nN: " << NLabel << "\n---" << std::string(NLabel.size(), '-')
         << '\n';
      for (auto Cell : Entry.second.cellVec()) {
        OS << "D1: " << DToString(Cell.r) << '\n'
           << "\tD2: " << DToString(Cell.c) << '\n'
           << "\tEF: " << Cell.v << "\n\n";
      }
    }
  }

  void printNonEmptyReverseLookupImpl(llvm::raw_ostream &OS) {
    OS << "DUMP nonEmptyReverseLookup\nTable<N, D, "
          "phmap::parallel_node_hash_map_m<D, "
          "EdgeFunctionPtrType>>\n";
    auto CellVec = NonEmptyReverseLookup.cellVec();
    for (auto Cell : CellVec) {
      OS << "N : " << NToString(Cell.r) << "\nD1: " << DToString(Cell.c)
         << '\n';
      for (auto D2ToEF : Cell.v) {
        OS << "D2: " << DToString(D2ToEF.first) << "\nEF: " << D2ToEF.second
           << '\n';
      }
      OS << '\n';
    }
  }

  void printNonEmptyLookupByTargetNodeImpl(llvm::raw_ostream &OS) {
    OS << "DUMP nonEmptyLookupByTargetNode\nphmap::parallel_node_hash_map_m<N, "
          "Table<D, D, "
          "EdgeFunctionPtrType>>\n";
    for (auto Node : NonEmptyLookupByTargetNode) {
      OS << "\nN : " << NToString(Node.first) << '\n';
      auto Table = NonEmptyLookupByTargetNode[Node.first];
      auto CellVec = Table.cellVec();
      for (auto Cell : CellVec) {
        OS << "D1: " << DToString(Cell.r) << "\nD2: " << DToString(Cell.c)
           << "\nEF: " << Cell.v << '\n';
      }
      OS << '\n';
    }
  }

public:
  /**
   * Removes all jump functions
   */
  void clear() {
    if constexpr (has_for_each<const Container>) {
      std::lock_guard ReverseGuard(NonEmptyReverseLookupMutex);
      std::lock_guard ForwardGuard(NonEmptyForwardLookupMutex);
      std::lock_guard LookupGuard(NonEmptyLookupByTargetNodeMutex);

      clearImpl();
    } else {
      clearImpl();
    }
  }

  void printJumpFunctions(llvm::raw_ostream &OS) {
    if constexpr (has_for_each<const Container>) {
      std::lock_guard Guard(NonEmptyLookupByTargetNodeMutex);

      printJumpFunctionsImpl(OS);
    } else {
      printJumpFunctionsImpl(OS);
    }
  }

  void printNonEmptyReverseLookup(llvm::raw_ostream &OS) {
    if constexpr (has_for_each<const Container>) {
      std::lock_guard Guard(NonEmptyReverseLookupMutex);

      printNonEmptyReverseLookupImpl(OS);
    } else {
      printNonEmptyReverseLookupImpl(OS);
    }
  }

  void printNonEmptyForwardLookup(llvm::raw_ostream &OS) {
    // Not moving these to an Impl function like the other above, because the
    // lock_guard is in an unfortunate spot. I don't want to overcomplicate
    // this.
    if constexpr (has_for_each<const Container>) {
      OS << "DUMP nonEmptyForwardLookup\nTable<D, N, "
            "phmap::parallel_node_hash_map_m<D, "
            "EdgeFunctionPtrType>>\n";
      using TableCell =
          typename Table<d_t, n_t,
                         std::unordered_map<d_t, EdgeFunction<l_t>>>::Cell;
      std::vector<TableCell> CellVec;
      {
        std::lock_guard Guard(NonEmptyForwardLookupMutex);
        CellVec = NonEmptyForwardLookup.cellVec();
      }
      for (auto Cell : CellVec) {
        OS << "D1: " << DToString(Cell.r) << "\nN : " << NToString(Cell.c)
           << '\n';
        for (auto D2ToEF : Cell.v) {
          OS << "D2: " << DToString(D2ToEF.first) << "\nEF: " << D2ToEF.second
             << '\n';
        }
        OS << '\n';
      }
    } else {
      OS << "DUMP nonEmptyForwardLookup\nTable<D, N, std::unordered_map<D, "
            "EdgeFunctionPtrType>>\n";
      auto CellVec = NonEmptyForwardLookup.cellVec();
      for (auto Cell : CellVec) {
        OS << "D1: " << DToString(Cell.r) << "\nN : " << NToString(Cell.c)
           << '\n';
        for (auto D2ToEF : Cell.v) {
          OS << "D2: " << DToString(D2ToEF.first) << "\nEF: " << D2ToEF.second
             << '\n';
        }
        OS << '\n';
      }
    }
  }

  void printNonEmptyLookupByTargetNode(llvm::raw_ostream &OS) {
    if constexpr (has_for_each<const Container>) {
      std::lock_guard Guard(NonEmptyLookupByTargetNodeMutex);
      printNonEmptyLookupByTargetNodeImpl(OS);
    } else {
      printNonEmptyLookupByTargetNodeImpl(OS);
    }
  }
};

} // namespace psr

#endif
