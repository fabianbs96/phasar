/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * JumpFunctionsPll.h
 *
 *  Created on: 10.04.2026
 *      Author: mxHuber
 */

#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_JUMPFUNCTIONS_PLL_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_JUMPFUNCTIONS_PLL_H

#include "phasar/DataFlow/IfdsIde/EdgeFunctionUtils.h"
#include "phasar/DataFlow/IfdsIde/IfdsIdeDomain.h"
#include "phasar/Utils/ByRef.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/Table.h"

#include "llvm/ADT/SmallVector.h"

#include "parallel_hashmap/phmap.h"
#include "parallel_hashmap/phmap_fwd_decl.h"

#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <ostream>
#include <unordered_map>
#include <utility>

namespace psr {

template <typename AnalysisDomainTy, typename Container>
class JumpFunctionsPll {
public:
  using l_t = typename AnalysisDomainTy::l_t;
  using d_t = typename AnalysisDomainTy::d_t;
  using n_t = typename AnalysisDomainTy::n_t;

  // N=7 (128 shards) instead of the default N=4 (16 shards): with the thread
  // pool defaulting to hardware_concurrency() threads, 16 shards causes heavy
  // per-shard mutex contention; more shards trades a small constant memory
  // overhead per map for far fewer collisions.
  template <typename Key, typename Val>
  using PllMap = phmap::parallel_node_hash_map_m<
      Key, Val, phmap::Hash<Key>, phmap::EqualTo<Key>,
      phmap::Allocator<std::pair<const Key, Val>>, 7>;

protected:
  // mapping from target node and value to a list of all source values and
  // associated functions where the list is implemented as a mapping from
  // the source value to the function we exclude empty default functions
  Table<n_t, d_t, llvm::SmallVector<std::pair<d_t, EdgeFunction<l_t>>, 1>,
        PllMap>
      NonEmptyReverseLookup;
  // mapping from source value and target node to a list of all target values
  // and associated functions where the list is implemented as a mapping from
  // the source value to the function we exclude empty default functions
  Table<d_t, n_t, llvm::SmallVector<std::pair<d_t, EdgeFunction<l_t>>, 1>,
        PllMap>
      NonEmptyForwardLookup;
  // a mapping from target node to a list of triples consisting of source value,
  // target value and associated function; the triple is implemented by a table
  // we exclude empty default functions
  phmap::parallel_node_hash_map_m<n_t,
                                  Table<d_t, d_t, EdgeFunction<l_t>, PllMap>>
      NonEmptyLookupByTargetNode;

  std::mutex NonEmptyReverseLookupMutex;
  std::mutex NonEmptyForwardLookupMutex;
  std::mutex NonEmptyLookupByTargetNodeMutex;

public:
  JumpFunctionsPll() noexcept = default;
  ~JumpFunctionsPll() = default;

  JumpFunctionsPll(const JumpFunctionsPll &JFs) = default;
  JumpFunctionsPll &operator=(const JumpFunctionsPll &JFs) = default;
  JumpFunctionsPll(JumpFunctionsPll &&JFs) noexcept = default;
  JumpFunctionsPll &operator=(JumpFunctionsPll &&JFs) noexcept = default;

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
        // it is important that existing values in JumpFunctionsPll
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
        // it is important that existing values in JumpFunctionsPll
        // are overwritten
        Find->second = EdgeFunc;
      } else {
        TargetValToFunc.emplace_back(TargetVal, EdgeFunc);
      }
    });

    // V Table::insert(R r, C c, V v) always overrides (see
    // comments above)
    auto &Inner =
        NonEmptyLookupByTargetNode.try_emplace_p(Target).first->second;
    Inner.insert(SourceVal, TargetVal, EdgeFunc);

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
        // it is important that existing values in JumpFunctionsPll
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
   * TODO: add more context
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
   * TODO: add more context
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
  // No locking needed: operator[] on the underlying parallel_node_hash_map_m
  // is already thread-safe per-key (find-or-insert happens under that key's
  // shard lock), and Target entries are only ever added, never erased, while
  // solving.
  Table<d_t, d_t, EdgeFunction<l_t>, PllMap> &lookupByTarget(n_t Target) {
    return NonEmptyLookupByTargetNode[Target];
  }

  template <typename HandlerFn>
  void foreachEdgeFunction(HandlerFn Handler) const {
    std::lock_guard Guard(NonEmptyForwardLookupMutex);
    NonEmptyForwardLookup.foreachCell(
        [Handler = std::move(Handler)](ByConstRef<d_t> /*Row*/,
                                       ByConstRef<n_t> /*Col*/,
                                       const auto &TargetFactAndEF) {
          for (const auto &[TargetFact, EF] : TargetFactAndEF) {
            std::invoke(Handler, EF);
          }
        });
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

    std::lock_guard LookupGuard(NonEmptyLookupByTargetNodeMutex);
    return NonEmptyLookupByTargetNode.erase(Target);
  }

  /**
   * Removes all jump functions
   */
  void clear() {
    std::lock_guard ReverseGuard(NonEmptyReverseLookupMutex);
    std::lock_guard ForwardGuard(NonEmptyForwardLookupMutex);
    std::lock_guard LookupGuard(NonEmptyLookupByTargetNodeMutex);

    NonEmptyReverseLookup.clear();
    NonEmptyForwardLookup.clear();
    NonEmptyLookupByTargetNode.clear();
  }

  void printJumpFunctionsPll(llvm::raw_ostream &OS) {
    OS << "\n******************************************************";
    OS << "\n*              Print all Jump Functions              *";
    OS << "\n******************************************************\n";
    std::lock_guard Guard(NonEmptyLookupByTargetNodeMutex);
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

  void printNonEmptyReverseLookup(llvm::raw_ostream &OS) {
    OS << "DUMP nonEmptyReverseLookup\nTable<N, D, "
          "phmap::parallel_node_hash_map_m<D, "
          "EdgeFunctionPtrType>>\n";
    std::lock_guard Guard(NonEmptyReverseLookupMutex);
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

  void printNonEmptyForwardLookup(llvm::raw_ostream &OS) {
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
  }

  void printNonEmptyLookupByTargetNode(llvm::raw_ostream &OS) {
    OS << "DUMP nonEmptyLookupByTargetNode\nphmap::parallel_node_hash_map_m<N, "
          "Table<D, D, "
          "EdgeFunctionPtrType>>\n";
    std::lock_guard Guard(NonEmptyLookupByTargetNodeMutex);
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
};

} // namespace psr

#endif
