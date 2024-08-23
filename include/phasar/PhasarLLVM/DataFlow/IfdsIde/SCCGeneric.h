/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#pragma once

// #include "phasar/PhasarLLVM/Utils/Compressor.h"

#include "phasar/Utils/Utilities.h"

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/HashBuilder.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>

namespace psr {
class LLVMBasedICFG;
} // namespace psr

namespace psr::analysis::call_graph {
// struct TypeAssignmentGraph;
enum class GraphNodeId : uint32_t;

enum class [[clang::enum_extensibility(open)]] SCCId : uint32_t{};

// holds the scc's of a given graph
struct SCCHolder {
  llvm::SmallVector<SCCId, 0> SCCOfNode{};
  llvm::SmallVector<llvm::SmallVector<GraphNodeId, 1>> NodesInSCC{};
  size_t NumSCCs = 0;
};

// holds a graph were the scc's are compressed to a single node. Resulting graph
// is a DAG
struct SCCCallers {
  llvm::SmallVector<llvm::SmallDenseSet<SCCId>, 0> ChildrenOfSCC{};
  llvm::SmallVector<SCCId, 0> SCCRoots{};

  template <typename G>
  void print(llvm::raw_ostream &OS, const SCCHolder &SCCs, const G &Graph);
};

// holds topologically sorted scccallers
struct SCCOrder {
  llvm::SmallVector<SCCId, 0> SCCIds;
};

struct SCCData {
  llvm::SmallVector<uint32_t, 128> Disc;
  llvm::SmallVector<uint32_t, 128> Low;
  llvm::SmallBitVector OnStack;
  llvm::SmallVector<GraphNodeId> Stack;
  uint32_t Time = 0;
  llvm::SmallBitVector Seen;

  explicit SCCData(size_t NumFuns)
      : Disc(NumFuns, UINT32_MAX), Low(NumFuns, UINT32_MAX), OnStack(NumFuns),
        Seen(NumFuns) {}
};

struct SCCDataIt {
  llvm::SmallVector<uint32_t, 128> Disc;
  llvm::SmallVector<uint32_t, 128> Low;
  llvm::SmallBitVector OnStack;
  llvm::SmallVector<GraphNodeId> Stack;
  llvm::SmallVector<std::pair<GraphNodeId, uint32_t>> CallStack;
  uint32_t Time = 0;
  llvm::SmallBitVector Seen;

  explicit SCCDataIt(size_t NumFuns)
      : Disc(NumFuns, UINT32_MAX), Low(NumFuns, UINT32_MAX), OnStack(NumFuns),
        Seen(NumFuns) {}
};

static void setMin(uint32_t &InOut, uint32_t Other) {
  if (Other < InOut) {
    InOut = Other;
  }
}

// TODO: Non-recursive version
template <typename G>
static void computeSCCsRec(const G &Graph, GraphNodeId CurrNode, SCCData &Data,
                           SCCHolder &Holder) {
  // See
  // https://www.geeksforgeeks.org/tarjan-algorithm-find-strongly-connected-components

  auto CurrTime = Data.Time++;
  Data.Disc[size_t(CurrNode)] = CurrTime;
  Data.Low[size_t(CurrNode)] = CurrTime;
  Data.Stack.push_back(CurrNode);
  Data.OnStack.set(uint32_t(CurrNode));

  for (auto SuccNode : Graph.Adj[size_t(CurrNode)]) {
    if (Data.Disc[size_t(SuccNode)] == UINT32_MAX) {
      // Tree-edge: Not seen yet --> recurse

      computeSCCsRec(Graph, SuccNode, Data, Holder);
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

// Iterative IMplementation for Tarjan's SCC Alg.
// -> Heapoverflow through simulated Stack?
template <typename G>
static void TarjanIt(const G &Graph, GraphNodeId StartNode, SCCDataIt &Data,
                     SCCHolder &Holder) {

  auto CurrTime = Data.Time;
  for (uint32_t Vertex = 0; Vertex < Graph.Nodes.size(); Vertex++) {
    if (Data.Disc[size_t(Vertex)] == UINT32_MAX) {
      Data.CallStack.push_back({GraphNodeId(Vertex), 0});
      while (!Data.CallStack.empty()) {
        auto Curr = Data.CallStack.pop_back_val();
        // Curr.second = 0 implies that Curr.fist was not visited before
        if (Curr.second == 0) {
          Data.Disc[size_t(Curr.first)] = CurrTime;
          Data.Low[size_t(Curr.first)] = CurrTime;
          CurrTime++;
          Data.Stack.push_back(Curr.first);
          Data.OnStack.set(uint32_t(Curr.first));
        }
        // Curr.second > 0 implies that we came back from a recursive call
        if (Curr.second > 0) {
          //???
          setMin(Data.Low[size_t(Curr.first)],
                 Data.Low[size_t(Curr.second) - 1]);
        }
        // find the next recursive function call
        while (Curr.second < Graph.getEdges(Curr.first).size() &&
               Data.Disc[size_t(Graph.getEdges(Curr.first)[Curr.second])]) {
          GraphNodeId W = Graph.getEdges(Curr.first)[Curr.second];
          if (Data.OnStack.test(uint32_t(W))) {
            setMin(Data.Low[size_t(Curr.first)], Data.Disc[size_t(W)]);
          }
          Curr.second++;
          // If a Node u is undiscovered i.e. Data.Disc[size_t(u)] = UINT32_MAX
          // start a recursive function call
          if (Curr.second < Graph.getEdges(Curr.first).size()) {
            GraphNodeId U = Graph.getEdges(Curr.first)[Curr.second];
            Data.CallStack.push_back({Curr.first, Curr.second++});
            Data.CallStack.push_back({U, 0});
          }
          // If Curr.first is the root of a connected component i.e. Data.Disc =
          // Data.Low
          if (Data.Low[size_t(Curr.first)] == Data.Disc[size_t(Curr.first)]) {
            //-> SCC found
            auto SCCIdx = SCCId(Holder.NumSCCs++);
            auto &NodesInSCC = Holder.NodesInSCC.emplace_back();

            assert(!Data.Stack.empty());

            while (Data.Stack.back() != Curr.first) {
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
      }
    }
  }
}

template <typename G> [[nodiscard]] SCCHolder computeSCCs(const G &Graph) {
  SCCHolder Ret{};

  auto NumNodes = Graph.Nodes.size();
  Ret.SCCOfNode.resize(NumNodes);

  if (!NumNodes) {
    return Ret;
  }

  SCCData Data(NumNodes);
  for (uint32_t FunId = 0; FunId != NumNodes; ++FunId) {
    if (!Data.Seen.test(FunId)) {
      computeSCCsRec(Graph, GraphNodeId(FunId), Data, Ret);
    }
  }

  return Ret;
}

// choose which Tarjan implementation will be executed
template <typename G>
[[nodiscard]] SCCHolder execTarjan(const G &Graph, const bool Iterative) {
  SCCHolder Ret{};

  auto NumNodes = Graph.Nodes.size();
  Ret.SCCOfNode.resize(NumNodes);

  if (!NumNodes) {
    return Ret;
  }

  SCCData Data(NumNodes);
  for (uint32_t FunId = 0; FunId != NumNodes; ++FunId) {
    if (!Data.Seen.test(FunId)) {
      if (Iterative) {
        TarjanIt(Graph, GraphNodeId(FunId), Data, Ret);
      } else {
        computeSCCsRec(Graph, GraphNodeId(FunId), Data, Ret);
      }
    }
  }

  return Ret;
}

template <typename G>
[[nodiscard]] LLVM_LIBRARY_VISIBILITY SCCCallers
computeSCCCallers(const G &Graph, const SCCHolder &SCCs);

template <typename G>
auto computeSCCCallers(const G &Graph, const SCCHolder &SCCs) -> SCCCallers {
  SCCCallers Ret;
  Ret.ChildrenOfSCC.resize(SCCs.NumSCCs);

  llvm::SmallBitVector Roots(SCCs.NumSCCs, true);

  size_t NodeId = 0;
  for (const auto &SuccNodes : Graph.Adj) {
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

template <typename G>
void analysis::call_graph::SCCCallers::print(llvm::raw_ostream &OS,
                                             const SCCHolder &SCCs,
                                             const G &Graph) {
  OS << "digraph SCCTAG {\n";
  psr::scope_exit CloseBrace = [&OS] { OS << "}\n"; };
  for (size_t Ctr = 0; Ctr != SCCs.NumSCCs; ++Ctr) {
    OS << "  " << Ctr << "[label=\"";
    for (auto TNId : SCCs.NodesInSCC[Ctr]) {
      auto TN = Graph.Nodes[TNId];
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

[[nodiscard]] LLVM_LIBRARY_VISIBILITY SCCOrder
computeSCCOrder(const SCCHolder &SCCs, const SCCCallers &Callers);

inline auto computeSCCOrder(const SCCHolder &SCCs, const SCCCallers &Callers)
    -> SCCOrder {
  SCCOrder Ret;
  Ret.SCCIds.reserve(SCCs.NumSCCs);

  llvm::SmallBitVector Seen;
  Seen.resize(SCCs.NumSCCs);

  auto Dfs = [&](auto &Dfs, SCCId CurrSCC) -> void {
    Seen.set(uint32_t(CurrSCC));
    for (auto Caller : Callers.ChildrenOfSCC[size_t(CurrSCC)]) {
      if (!Seen.test(uint32_t(Caller))) {
        Dfs(Dfs, Caller);
      }
    }
    Ret.SCCIds.push_back(CurrSCC);
  };

  for (auto Leaf : Callers.SCCRoots) {
    if (!Seen.test(uint32_t(Leaf))) {
      Dfs(Dfs, Leaf);
    }
  }

  std::reverse(Ret.SCCIds.begin(), Ret.SCCIds.end());

  return Ret;
}
} // namespace psr::analysis::call_graph

namespace llvm {
template <> struct DenseMapInfo<psr::analysis::call_graph::SCCId> {
  using SCCId = psr::analysis::call_graph::SCCId;

  static inline SCCId getEmptyKey() noexcept { return SCCId(-1); }
  static inline SCCId getTombstoneKey() noexcept { return SCCId(-2); }
  static inline auto getHashValue(SCCId Id) noexcept {
    return llvm::hash_value(uint32_t(Id));
  }
  static inline bool isEqual(SCCId L, SCCId R) noexcept { return L == R; }
};
} // namespace llvm
