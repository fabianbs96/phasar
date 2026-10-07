#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/DataFlow/IfdsIde/Solver/DFISolverResults.h"
#include "phasar/Utils/BitSet.h"
#include "phasar/Utils/CsrGraph.h"
#include "phasar/Utils/FunctionId.h"
#include "phasar/Utils/IntervalReachability.h"
#include "phasar/Utils/SCCGeneric.h"
#include "phasar/Utils/SCCId.h"
#include "phasar/Utils/StrongTypeDef.h"
#include "phasar/Utils/TypedVector.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"

#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

/// A function entry <StartPoint, Fact> reached by at least one call
PHASAR_STRONG_TYPEDEF(psr::dfi, uint32_t, EntryId);
/// Index of a vertex among the vertices of its function
PHASAR_STRONG_TYPEDEF(psr::dfi, uint32_t, LocalVertexId);

namespace psr::detail {

/// The exploded supergraph under construction by the DFISolver.
///
/// Works on ids only; evaluating flow functions is up to the solver. Tracks
/// which vertices still need to be expanded and which functions need to be
/// summarized, and schedules this work callees first.
class DFIExplodedSupergraph {
public:
  enum class ActionKind { Expand, Summarize };

  struct Action {
    FunctionId Fun{};
    ActionKind Kind{};
  };

  /// \param CGSCCs The SCCs of the call-graph in reverse topological order,
  /// as computed by computeCGSCCs()
  explicit DFIExplodedSupergraph(SCCHolder<FunctionId> CGSCCs);

  /// The next step towards the fixpoint, or std::nullopt if there is none.
  /// Prefers callees over callers, and expansion over summarization.
  [[nodiscard]] std::optional<Action> nextAction();

  /// Returns the vertex <Inst, Fact>. New vertices are scheduled for
  /// expansion.
  dfi::VertexId getOrCreateVertex(dfi::InstId Inst, dfi::FactId Fact,
                                  FunctionId Fun, bool IsExit);

  /// Marks Vtx as seed. Returns false, if it was a seed already.
  bool addSeed(dfi::VertexId Vtx);

  /// Marks the vertex Vtx, which must be at a start point, as function entry.
  dfi::EntryId getOrCreateEntry(dfi::VertexId Vtx);

  /// Removes the vertices scheduled for expansion in Fun, sorted by
  /// instruction. The result is valid until the next call of this function.
  [[nodiscard]] llvm::ArrayRef<dfi::VertexId> takeWorklist(FunctionId Fun);

  /// Records the intra-procedural out-edges of Vtx: call beginEdges(Vtx), then
  /// addEdge() for each target, then endEdges(Vtx). Vtx must not be expanded
  /// before. Creating vertices in between is fine.
  void beginEdges(dfi::VertexId Vtx) {
    VtxEdgeBegin[Vtx] = uint32_t(Edges.size());
  }
  void addEdge(dfi::VertexId Target) { Edges.push_back(Target); }
  void endEdges(dfi::VertexId Vtx) { VtxEdgeEnd[Vtx] = uint32_t(Edges.size()); }

  /// Adds a summary edge from the call vertex CallVtx to the return-site
  /// vertex RetVtx. Returns false, if the edge was already there.
  bool addSummaryEdge(dfi::VertexId CallVtx, dfi::VertexId RetVtx);

  /// Adds a call edge from CallVtx to Entry. Returns the exits already known
  /// to be reachable from Entry; the solver must apply them to CallVtx. The
  /// result is invalidated by getOrCreateEntry().
  [[nodiscard]] llvm::ArrayRef<dfi::VertexId> addCallEdge(dfi::VertexId CallVtx,
                                                          dfi::EntryId Entry);

  /// Computes the same-level reachability from the entries of Fun to its exit
  /// vertices and calls Handler(CallVtx, Entry, Exit) for each new
  /// (Entry, Exit) pair and each call vertex CallVtx calling Entry.
  void summarize(
      FunctionId Fun,
      std::invocable<dfi::VertexId, dfi::EntryId, dfi::VertexId> auto Handler) {
    indexFunction(Fun);

    for (auto Entry : FunState[Fun].Entries) {
      auto NewExits = updateSummaryExits(Fun, Entry);
      // Handler may add vertices and summary edges, but no entries or callers
      for (size_t I = 0; I != EntryCallers[Entry].size(); ++I) {
        for (auto Exit : NewExits) {
          std::invoke(Handler, EntryCallers[Entry][I], Entry, Exit);
        }
      }
    }
  }

  [[nodiscard]] size_t numVertices() const noexcept { return VtxInst.size(); }
  [[nodiscard]] dfi::InstId instOf(dfi::VertexId Vtx) const noexcept {
    return VtxInst[Vtx];
  }
  [[nodiscard]] dfi::FactId factOf(dfi::VertexId Vtx) const noexcept {
    return VtxFact[Vtx];
  }
  [[nodiscard]] FunctionId funOf(dfi::VertexId Vtx) const noexcept {
    return VtxFun[Vtx];
  }
  [[nodiscard]] FunctionId funOf(dfi::EntryId Entry) const noexcept {
    return VtxFun[EntryVertex[Entry]];
  }

  [[nodiscard]] llvm::ArrayRef<dfi::VertexId> seeds() const noexcept {
    return SeedVertices;
  }

  /// The graph of intra-procedural, summary, and call edges
  [[nodiscard]] CsrGraph<dfi::VertexId> buildSupergraph() const;

  [[nodiscard]] TypedVector<dfi::VertexId, dfi::InstId>
  takeVertexInsts() noexcept {
    return std::move(VtxInst);
  }
  [[nodiscard]] TypedVector<dfi::VertexId, dfi::FactId>
  takeVertexFacts() noexcept {
    return std::move(VtxFact);
  }

private:
  struct FunctionState {
    TypedVector<dfi::LocalVertexId, dfi::VertexId> Vertices;
    llvm::SmallVector<dfi::VertexId, 0> Worklist;
    /// Entries and seeds
    llvm::SmallVector<dfi::VertexId, 0> Roots;
    llvm::SmallVector<dfi::EntryId, 0> Entries;
    llvm::SmallVector<std::pair<dfi::VertexId, dfi::VertexId>, 0> SummaryEdges;
    /// Whether the graph changed since the last summarize()
    bool Dirty = false;
  };

  void markDirty(FunctionId Fun);
  bool addRoot(dfi::VertexId Vtx);

  /// Builds LocalIndex for the vertices of Fun, with exits as targets
  void indexFunction(FunctionId Fun);

  /// Adds the exits reachable from Entry according to LocalIndex to its
  /// summary. Returns the newly added ones.
  [[nodiscard]] llvm::ArrayRef<dfi::VertexId>
  updateSummaryExits(FunctionId Fun, dfi::EntryId Entry);

  [[nodiscard]] llvm::ArrayRef<dfi::VertexId>
  edgesOf(dfi::VertexId Vtx) const noexcept {
    auto Begin = VtxEdgeBegin[Vtx];
    return llvm::ArrayRef<dfi::VertexId>(Edges).slice(Begin,
                                                      VtxEdgeEnd[Vtx] - Begin);
  }

  SCCHolder<FunctionId> CGSCCs;
  BitSet<SCCId<FunctionId>> PendingSCCs;
  TypedVector<FunctionId, FunctionState> FunState;

  // Per vertex
  llvm::DenseMap<std::pair<dfi::InstId, dfi::FactId>, dfi::VertexId> VertexOf;
  TypedVector<dfi::VertexId, dfi::InstId> VtxInst;
  TypedVector<dfi::VertexId, dfi::FactId> VtxFact;
  TypedVector<dfi::VertexId, FunctionId> VtxFun;
  TypedVector<dfi::VertexId, dfi::LocalVertexId> VtxLocalId;
  /// Intra-procedural out-edges, excluding summary edges:
  /// Edges[VtxEdgeBegin[V] .. VtxEdgeEnd[V])
  TypedVector<dfi::VertexId, uint32_t> VtxEdgeBegin;
  TypedVector<dfi::VertexId, uint32_t> VtxEdgeEnd;
  BitSet<dfi::VertexId> IsExit;
  BitSet<dfi::VertexId> IsRoot;
  llvm::SmallVector<dfi::VertexId, 0> Edges;
  llvm::SmallVector<dfi::VertexId, 0> SeedVertices;

  // Per entry
  llvm::DenseMap<dfi::VertexId, dfi::EntryId> EntryOf;
  TypedVector<dfi::EntryId, dfi::VertexId> EntryVertex;
  TypedVector<dfi::EntryId, llvm::SmallVector<dfi::VertexId, 2>> EntryCallers;
  /// Exit vertices reachable from the entry, sorted
  TypedVector<dfi::EntryId, llvm::SmallVector<dfi::VertexId, 2>> SummaryExits;

  llvm::DenseSet<std::pair<dfi::VertexId, dfi::VertexId>> SummaryEdgeSet;
  llvm::SmallVector<std::pair<dfi::VertexId, dfi::VertexId>, 0> CallEdges;

  // Scratch buffers
  llvm::SmallVector<dfi::VertexId, 0> Batch;
  llvm::SmallVector<std::pair<dfi::LocalVertexId, dfi::LocalVertexId>, 0>
      LocalEdges;
  llvm::SmallVector<dfi::LocalVertexId, 0> LocalRoots;
  llvm::SmallVector<dfi::VertexId, 0> NewExits;
  CsrGraph<dfi::LocalVertexId> LocalGraph;
  IntervalReachabilityBuilder<dfi::LocalVertexId> LocalBuilder;
  IntervalReachabilityIndex<dfi::LocalVertexId> LocalIndex;
};

} // namespace psr::detail
