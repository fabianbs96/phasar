/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#include "SCC.h"

#include "llvm/ADT/SmallBitVector.h"

#include "../../../../../utils/include/phasar/Utils/Compressor.h"
#include "TypeAssignmentGraph.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

using namespace psr;

using SCCId = analysis::call_graph::SCCId;

struct SCCData {
  llvm::SmallVector<uint32_t, 128> Disc;
  llvm::SmallVector<uint32_t, 128> Low;
  llvm::SmallBitVector OnStack;
  llvm::SmallVector<analysis::call_graph::TAGNodeId> Stack;
  uint32_t Time = 0;
  llvm::SmallBitVector Seen;

  explicit SCCData(size_t NumFuns)
      : Disc(NumFuns, UINT32_MAX), Low(NumFuns, UINT32_MAX), OnStack(NumFuns),
        Seen(NumFuns) {}
};

static void setMin(uint32_t &InOut, uint32_t Other) {
  if (Other < InOut)
    InOut = Other;
}

// TODO: Non-recursive version
static void computeSCCsRec(const analysis::call_graph::TypeAssignmentGraph &TAG,
                           analysis::call_graph::TAGNodeId CurrNode,
                           SCCData &Data,
                           psr::analysis::call_graph::SCCHolder &Holder) {
  // See
  // https://www.geeksforgeeks.org/tarjan-algorithm-find-strongly-connected-components

  auto CurrTime = Data.Time++;
  Data.Disc[size_t(CurrNode)] = CurrTime;
  Data.Low[size_t(CurrNode)] = CurrTime;
  Data.Stack.push_back(CurrNode);
  Data.OnStack.set(uint32_t(CurrNode));

  for (auto SuccNode : TAG.Adj[size_t(CurrNode)]) {
    if (Data.Disc[size_t(SuccNode)] == UINT32_MAX) {
      // Tree-edge: Not seen yet --> recurse

      computeSCCsRec(TAG, SuccNode, Data, Holder);
      setMin(Data.Low[size_t(CurrNode)], Data.Low[size_t(SuccNode)]);
    } else if (Data.OnStack.test(uint32_t(SuccNode))) {
      // Back-edge --> circle!

      setMin(Data.Low[size_t(CurrNode)], Data.Disc[size_t(SuccNode)]);
    }
  }

  if (Data.Low[size_t(CurrNode)] == Data.Disc[size_t(CurrNode)]) {
    // Found SCC

    auto SCCIdx = SCCId(Holder.NumSCCs++);
    auto &NodesInSCC = Holder.NodesInSCC.emplace_back();

    assert(!Data.Stack.empty());

    while (Data.Stack.back() != CurrNode) {
      auto Fun = Data.Stack.pop_back_val();
      Holder.SCCOfNode[size_t(Fun)] = SCCIdx;
      Data.OnStack.reset(uint32_t(Fun));
      Data.Seen.set(uint32_t(Fun));
      NodesInSCC.push_back(Fun);
    }

    auto Fun = Data.Stack.pop_back_val();
    Holder.SCCOfNode[size_t(Fun)] = SCCIdx;
    Data.OnStack.reset(uint32_t(Fun));
    Data.Seen.set(uint32_t(Fun));
    NodesInSCC.push_back(Fun);
  }
}

auto analysis::call_graph::computeSCCs(const TypeAssignmentGraph &TAG)
    -> SCCHolder {
  SCCHolder Ret{};

  auto NumNodes = TAG.Nodes.size();
  Ret.SCCOfNode.resize(NumNodes);

  if (!NumNodes)
    return Ret;

  SCCData Data(NumNodes);
  for (uint32_t FunId = 0; FunId != NumNodes; ++FunId) {
    if (!Data.Seen.test(FunId))
      computeSCCsRec(TAG, TAGNodeId(FunId), Data, Ret);
  }

  return Ret;
}

auto analysis::call_graph::computeSCCCallers(const TypeAssignmentGraph &TAG,
                                             const SCCHolder &SCCs)
    -> SCCCallers {
  SCCCallers Ret;
  Ret.ChildrenOfSCC.resize(SCCs.NumSCCs);

  llvm::SmallBitVector Roots(SCCs.NumSCCs, true);

  size_t NodeId = 0;
  for (const auto &SuccNodes : TAG.Adj) {
    auto SrcSCC = SCCs.SCCOfNode[NodeId];

    for (auto SuccNode : SuccNodes) {
      auto DestSCC = SCCs.SCCOfNode[size_t(SuccNode)];
      if (DestSCC != SrcSCC) {
        Ret.ChildrenOfSCC[size_t(SrcSCC)].insert(DestSCC);
        Roots.reset(uint32_t(DestSCC));
      }
    }

    ++NodeId;
  }

  Ret.SCCRoots.reserve(Roots.count());
  for (auto Rt : Roots.set_bits()) {
    Ret.SCCRoots.push_back(SCCId(Rt));
  }

  return Ret;
}

void analysis::call_graph::SCCCallers::print(llvm::raw_ostream &OS,
                                             const SCCHolder &SCCs,
                                             const TypeAssignmentGraph &TAG) {
  OS << "digraph SCCTAG {\n";
  psr::scope_exit CloseBrace = [&OS] { OS << "}\n"; };
  for (size_t Ctr = 0; Ctr != SCCs.NumSCCs; ++Ctr) {
    OS << "  " << Ctr << "[label=\"";
    for (auto TNId : SCCs.NodesInSCC[Ctr]) {
      auto TN = TAG.Nodes[TNId];
      printNode(OS, TN);
      OS << "\\n";
    }
    OS << "\"];\n";
  }

  OS << '\n';

  size_t Ctr = 0;
  for (const auto &Targets : ChildrenOfSCC) {
    for (auto Tgt : Targets) {
      OS << "  " << Ctr << "->" << uint32_t(Tgt) << ";\n";
    }
    ++Ctr;
  }
}

auto analysis::call_graph::computeSCCOrder(const SCCHolder &SCCs,
                                           const SCCCallers &Callers)
    -> SCCOrder {
  SCCOrder Ret;
  Ret.SCCIds.reserve(SCCs.NumSCCs);

  llvm::SmallBitVector Seen;
  Seen.resize(SCCs.NumSCCs);

  auto Dfs = [&](auto &Dfs, SCCId CurrSCC) -> void {
    Seen.set(uint32_t(CurrSCC));
    for (auto Caller : Callers.ChildrenOfSCC[size_t(CurrSCC)]) {
      if (!Seen.test(uint32_t(Caller)))
        Dfs(Dfs, Caller);
    }
    Ret.SCCIds.push_back(CurrSCC);
  };

  for (auto Leaf : Callers.SCCRoots) {
    if (!Seen.test(uint32_t(Leaf)))
      Dfs(Dfs, Leaf);
  }

  std::reverse(Ret.SCCIds.begin(), Ret.SCCIds.end());

  return Ret;
}
