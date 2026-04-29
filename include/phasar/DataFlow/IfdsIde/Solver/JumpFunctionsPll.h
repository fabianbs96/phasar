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
#include "phasar/Utils/TablePll.h"

#include "llvm/ADT/SmallVector.h"

#include "parallel_hashmap/phmap.h"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <utility>

namespace psr {

template <typename AnalysisDomainTy, typename Container>
class JumpFunctionsPll {
public:
  using l_t = typename AnalysisDomainTy::l_t;
  using d_t = typename AnalysisDomainTy::d_t;
  using n_t = typename AnalysisDomainTy::n_t;

protected:
  // mapping from target node and value to a list of all source values and
  // associated functions where the list is implemented as a mapping from
  // the source value to the function we exclude empty default functions
  TablePll<n_t, d_t, llvm::SmallVector<std::pair<d_t, EdgeFunction<l_t>>, 1>>
      NonEmptyReverseLookup;
  // mapping from source value and target node to a list of all target values
  // and associated functions where the list is implemented as a mapping from
  // the source value to the function we exclude empty default functions
  TablePll<d_t, n_t, llvm::SmallVector<std::pair<d_t, EdgeFunction<l_t>>, 1>>
      NonEmptyForwardLookup;
  // a mapping from target node to a list of triples consisting of source value,
  // target value and associated function; the triple is implemented by a table
  // we exclude empty default functions
  phmap::parallel_node_hash_map_m<n_t, TablePll<d_t, d_t, EdgeFunction<l_t>>>
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

    {
      std::lock_guard Guard(NonEmptyReverseLookupMutex);
      auto &SourceValToFunc = NonEmptyReverseLookup.get(Target, TargetVal);
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
    }
    {
      std::lock_guard Guard(NonEmptyForwardLookupMutex);
      auto &TargetValToFunc = NonEmptyForwardLookup.get(SourceVal, Target);
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
    }

    // V Table::insert(R r, C c, V v) always overrides (see
    // comments above)
    {
      std::lock_guard Guard(NonEmptyLookupByTargetNodeMutex);
      NonEmptyLookupByTargetNode[Target].insert(SourceVal, TargetVal, EdgeFunc);
    }
    PHASAR_LOG_LEVEL(DEBUG, "End adding new jump function");
  }

  /**
   * Returns, for a given target statement and value all associated
   * source values, and for each the associated edge function.
   * The return value is a mapping from source value to function.
   */
  std::optional<std::reference_wrapper<
      llvm::SmallVectorImpl<std::pair<d_t, EdgeFunction<l_t>>>>>
  reverseLookup(n_t Target, d_t TargetVal) {
    std::lock_guard Guard(NonEmptyReverseLookupMutex);
    if (!NonEmptyReverseLookup.contains(Target, TargetVal)) {
      return std::nullopt;
    }
    return {NonEmptyReverseLookup.get(Target, TargetVal)};
  }

  /**
   * Returns, for a given source value and target statement all
   * associated target values, and for each the associated edge function.
   * The return value is a mapping from target value to function.
   */
  std::optional<std::reference_wrapper<
      llvm::SmallVectorImpl<std::pair<d_t, EdgeFunction<l_t>>>>>
  forwardLookup(d_t SourceVal, n_t Target) {
    std::lock_guard Guard(NonEmptyForwardLookupMutex);
    if (!NonEmptyForwardLookup.contains(SourceVal, Target)) {
      return std::nullopt;
    }
    return {NonEmptyForwardLookup.get(SourceVal, Target)};
  }

  /**
   * Returns for a given target statement all jump function records with this
   * target.
   * The return value is a set of records of the form
   * (sourceVal,targetVal,edgeFunction).
   */
  TablePll<d_t, d_t, EdgeFunction<l_t>> &lookupByTarget(n_t Target) {
    std::lock_guard Guard(NonEmptyLookupByTargetNodeMutex);
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
    {
      std::lock_guard Guard(NonEmptyReverseLookupMutex);
      auto &SourceValToFunc = NonEmptyReverseLookup.get(Target, TargetVal);
      if (auto Find = std::find_if(
              SourceValToFunc.begin(), SourceValToFunc.end(),
              [SourceVal](const std::pair<d_t, EdgeFunction<l_t>> &Entry) {
                return SourceVal == Entry.first;
              });
          Find != SourceValToFunc.end()) {
        SourceValToFunc.erase(Find);
      }
    }
    {
      std::lock_guard ForwardGuard(NonEmptyForwardLookupMutex);
      auto &TargetValToFunc = NonEmptyForwardLookup.get(SourceVal, Target);
      if (auto Find = std::find_if(
              TargetValToFunc.begin(), TargetValToFunc.end(),
              [TargetVal](const std::pair<d_t, EdgeFunction<l_t>> &Entry) {
                return TargetVal == Entry.first;
              });
          Find != TargetValToFunc.end()) {
        TargetValToFunc.erase(Find);
      }
    }
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
    OS << "DUMP nonEmptyReverseLookup\nTablePll<N, D, "
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
    OS << "DUMP nonEmptyForwardLookup\nTablePll<D, N, "
          "phmap::parallel_node_hash_map_m<D, "
          "EdgeFunctionPtrType>>\n";
    std::vector<TableCell<
        d_t, n_t, phmap::parallel_node_hash_map_m<d_t, EdgeFunction<l_t>>>>
        CellVec;
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
          "TablePll<D, D, "
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
