/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#ifndef PHASAR_UTILS_SCCGENERIC_H
#define PHASAR_UTILS_SCCGENERIC_H

#include "phasar/PhasarLLVM/ControlFlow/TypeAssignmentGraph.h"

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/DenseSet.h"
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

enum class [[clang::enum_extensibility(open)]] SCCId : uint32_t{};

// holds the scc's of a given graph
template <typename GraphNodeId> struct SCCHolder {
  llvm::SmallVector<SCCId, 0> SCCOfNode{};
  llvm::SmallVector<llvm::SmallVector<GraphNodeId, 1>> NodesInSCC{};
  size_t NumSCCs = 0;
};

// holds a graph were the scc's are compressed to a single node. Resulting graph
// is a DAG
template <typename G> struct SCCCallers {
  llvm::SmallVector<llvm::SmallDenseSet<SCCId>, 0> ChildrenOfSCC{};
  llvm::SmallVector<SCCId, 0> SCCRoots{};

  void print(llvm::raw_ostream &OS,
             const SCCHolder<typename G::GraphNodeId> &SCCs, const G &Graph);
};

// holds topologically sorted scccallers
struct SCCOrder {
  llvm::SmallVector<SCCId, 0> SCCIds;
};

template <typename GraphNodeId> struct SCCData {
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

template <typename GraphNodeId> struct SCCDataIt {
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

constexpr void setMin(uint32_t &InOut, uint32_t Other) {
  if (Other < InOut) {
    InOut = Other;
  }
}

// TODO: Non-recursive version
template <typename G>
static void computeSCCsRec(const G &Graph, typename G::GraphNodeId CurrNode,
                           SCCData<typename G::GraphNodeId> &Data,
                           SCCHolder<typename G::GraphNodeId> &Holder) {
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
static void tarjanIt(const G &Graph, SCCDataIt<typename G::GraphNodeId> &Data,
                     SCCHolder<typename G::GraphNodeId> &Holder) {
  using GraphNodeId = typename G::GraphNodeId;
  auto CurrTime = Data.Time;
  for (uint32_t Vertex = 0; Vertex < Graph.Adj.size(); Vertex++) {
    if (Data.Disc[Vertex] == UINT32_MAX) {
      Data.CallStack.push_back({GraphNodeId(Vertex), 0});
      while (!Data.CallStack.empty()) {
        auto Curr = Data.CallStack.pop_back_val();
        // Curr.second = 0 implies that node Curr.fist was not visited before
        if (Curr.second == 0) {
          Data.Disc[Curr.first] = CurrTime;
          Data.Low[Curr.first] = CurrTime;
          CurrTime++;
          Data.Stack.push_back(Curr.first);
          Data.OnStack.set(Curr.first);
        }
        // Curr.second > 0 implies that we came back from a recursive call of
        // node with higher depth
        if (Curr.second > 0) {
          setMin(Data.Low[Curr.first], Data.Low[Curr.second - 1]);
        }
        // find the next node for recursion
        while (Curr.second < Graph.getEdges(Curr.first).size() &&
               Data.Disc[Graph.getEdges(Curr.first)[Curr.second]] !=
                   UINT32_MAX) {
          GraphNodeId W = Graph.getEdges(Curr.first)[Curr.second];
          if (Data.OnStack.test(W)) {
            setMin(Data.Low[Curr.first], Data.Disc[W]);
          }
          Curr.second++;
          // If a Node u is undiscovered i.e. Data.Disc[u] = UINT32_MAX
          // start a recursive function call
          if (Curr.second < Graph.getEdges(Curr.first).size()) {
            GraphNodeId U = Graph.getEdges(Curr.first)[Curr.second];
            Data.CallStack.push_back({Curr.first, Curr.second++});
            Data.CallStack.push_back({U, 0});
          }
          // If Curr.first is the root of a connected component i.e. Data.Disc =
          // Data.Low i.e. cycle found
          if (Data.Low[Curr.first] == Data.Disc[Curr.first]) {
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

template <typename G>
[[nodiscard]] SCCHolder<typename G::GraphNodeId> computeSCCs(const G &Graph) {
  SCCHolder<typename G::GraphNodeId> Ret{};

  auto NumNodes = Graph.Adj.size();
  Ret.SCCOfNode.resize(NumNodes);

  if (!NumNodes) {
    return Ret;
  }

  SCCData Data(NumNodes);
  for (uint32_t FunId = 0; FunId != NumNodes; ++FunId) {
    if (!Data.Seen.test(FunId)) {
      computeSCCsRec(Graph, G::GraphNodeId(FunId), Data, Ret);
    }
  }

  return Ret;
}

// choose which Tarjan implementation will be executed
template <typename G>
[[nodiscard]] SCCHolder<typename G::GraphNodeId>
execTarjan(const G &Graph, const bool Iterative) {
  using GraphNodeId = typename G::GraphNodeId;
  SCCHolder<typename G::GraphNodeId> Ret{};

  auto NumNodes = Graph.Adj.size();
  Ret.SCCOfNode.resize(NumNodes);

  if (!NumNodes) {
    return Ret;
  }

  SCCData Data(NumNodes);
  SCCDataIt DataIt(NumNodes);
  for (uint32_t FunId = 0; FunId != NumNodes; ++FunId) {
    if (Iterative) {
      if (!DataIt.Senn.text(FunId)) {
        tarjanIt(Graph, DataIt, Ret);
      }
    } else {
      if (!Data.Seen.test(FunId)) {
        computeSCCsRec(Graph, GraphNodeId(FunId), Data, Ret);
      }
    }
  }

  return Ret;
}

template <typename G>
[[nodiscard]] SCCCallers<typename G::GraphNodeId>
computeSCCCallers(const G &Graph,
                  const SCCHolder<typename G::GraphNodeId> &SCCs);

template <typename G>
auto computeSCCCallers(const G &Graph,
                       const SCCHolder<typename G::GraphNodeId> &SCCs)
    -> SCCCallers<typename G::GraphNodeId> {
  SCCCallers<typename G::GraphNodeId> Ret;
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
[[nodiscard]] SCCOrder
computeSCCOrder(const SCCHolder<typename G::GraphNodeId> &SCCs,
                const SCCCallers<typename G::GraphNodeId> &Callers);
template <typename G>
inline auto computeSCCOrder(const SCCHolder<typename G::GraphNodeId> &SCCs,
                            const SCCCallers<typename G::GraphNodeId> &Callers)
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

#endif
