#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFGEdge.h"
#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFGNode.h"
#include "phasar/Utils/GraphTraits.h"
#include "phasar/Utils/IotaIterator.h"
#include "phasar/Utils/TypedVector.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

namespace psr {

/// Sparse Value Flow Graph: an inter-procedural, bidirectional graph that
/// makes value flows explicit across the whole program.
///
/// Nodes represent program points where a value is defined or used.
/// Edges are typed (Direct, Indirect, Call, Return).
///
/// Use SVFGBuilder to construct; call finalize() once building is complete.
class SVFG {
public:
  [[nodiscard]] SVFGNodeId addNode(SVFGNode N);
  void addEdge(SVFGNodeId From, SVFGNodeId To, SVFGEdgeKind Kind);
  /// Deduplicates edges and builds the reverse adjacency from the forward one.
  /// Must be called once before querying predEdges().
  void finalize();

  [[nodiscard]] llvm::ArrayRef<SVFGEdge> succEdges(SVFGNodeId N) const noexcept;
  [[nodiscard]] llvm::ArrayRef<SVFGEdge> predEdges(SVFGNodeId N) const noexcept;
  [[nodiscard]] const SVFGNode &node(SVFGNodeId N) const noexcept;
  [[nodiscard]] size_t numNodes() const noexcept { return Nodes.size(); }

  /// Returns all SVFG nodes anchored at the given IR value.
  /// There may be multiple (e.g., one ActualParam node per call site).
  [[nodiscard]] llvm::ArrayRef<SVFGNodeId>
  nodesFor(const llvm::Value *V) const noexcept;

  void printAsDot(llvm::raw_ostream &OS) const;
  [[nodiscard]] std::string exportAsDot() const;

private:
  TypedVector<SVFGNodeId, SVFGNode> Nodes;
  TypedVector<SVFGNodeId, llvm::SmallVector<SVFGEdge, 2>> Fwd;
  TypedVector<SVFGNodeId, llvm::SmallVector<SVFGEdge, 2>> Rev;
  llvm::DenseMap<const llvm::Value *, llvm::SmallVector<SVFGNodeId, 1>>
      ValueToNodes;
};

template <> struct GraphTraits<SVFG> {
  using graph_type = SVFG;
  using value_type = SVFGNode;
  using vertex_t = SVFGNodeId;
  using edge_t = SVFGEdge;

  static constexpr auto Invalid = llvm::DenseMapInfo<SVFGNodeId>::getEmptyKey();

  [[nodiscard]] static llvm::ArrayRef<SVFGEdge>
  outEdges(const SVFG &G, SVFGNodeId V) noexcept {
    return G.succEdges(V);
  }
  [[nodiscard]] static size_t outDegree(const SVFG &G, SVFGNodeId V) noexcept {
    return G.succEdges(V).size();
  }
  [[nodiscard]] static const SVFGNode &node(const SVFG &G,
                                            SVFGNodeId V) noexcept {
    return G.node(V);
  }
  [[nodiscard]] static SVFGNodeId target(SVFGEdge E) noexcept {
    return E.Target;
  }
  [[nodiscard]] static SVFGEdge withEdgeTarget(SVFGEdge E,
                                               SVFGNodeId T) noexcept {
    return {T, E.Kind};
  }
  [[nodiscard]] static auto vertices(const SVFG &G) noexcept {
    return iota<SVFGNodeId>(G.numNodes());
  }
  [[nodiscard]] static auto nodes(const SVFG &G) noexcept {
    return llvm::map_range(vertices(G), [&G](SVFGNodeId V) -> const SVFGNode & {
      return G.node(V);
    });
  }
  [[nodiscard]] static size_t size(const SVFG &G) noexcept {
    return G.numNodes();
  }
  [[nodiscard]] static llvm::ArrayRef<SVFGNodeId>
  roots(const SVFG & /*G*/) noexcept {
    return {};
  }
  [[nodiscard]] static size_t roots_size(const SVFG & /*G*/) noexcept {
    return 0;
  }
};

static_assert(is_const_graph_trait<GraphTraits<SVFG>>);

} // namespace psr
