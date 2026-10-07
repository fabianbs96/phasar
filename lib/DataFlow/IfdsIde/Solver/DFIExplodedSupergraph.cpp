/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/DataFlow/IfdsIde/Solver/DFIExplodedSupergraph.h"

#include "phasar/Utils/IotaIterator.h"

#include "llvm/ADT/STLExtras.h"

#include <algorithm>

using psr::detail::DFIExplodedSupergraph;

DFIExplodedSupergraph::DFIExplodedSupergraph(
    psr::SCCHolder<psr::FunctionId> CGSCCs)
    : CGSCCs(std::move(CGSCCs)) {
  FunState.resize(this->CGSCCs.SCCOfNode.size());
  PendingSCCs.reserve(this->CGSCCs.size());
}

auto DFIExplodedSupergraph::nextAction() -> std::optional<Action> {
  while (auto SCC = PendingSCCs.findFirst()) {
    const auto &FunsInSCC = CGSCCs.NodesInSCC[*SCC];
    for (auto Fun : FunsInSCC) {
      if (!FunState[Fun].Worklist.empty()) {
        return Action{Fun, ActionKind::Expand};
      }
    }
    for (auto Fun : FunsInSCC) {
      if (FunState[Fun].Dirty) {
        return Action{Fun, ActionKind::Summarize};
      }
    }
    PendingSCCs.erase(*SCC);
  }
  return std::nullopt;
}

psr::dfi::VertexId DFIExplodedSupergraph::getOrCreateVertex(dfi::InstId Inst,
                                                            dfi::FactId Fact,
                                                            FunctionId Fun,
                                                            bool IsExitVtx) {
  auto [It, Inserted] =
      VertexOf.try_emplace({Inst, Fact}, dfi::VertexId(VtxInst.size()));
  auto Vtx = It->second;
  if (!Inserted) {
    return Vtx;
  }

  auto &FS = FunState[Fun];
  VtxInst.push_back(Inst);
  VtxFact.push_back(Fact);
  VtxFun.push_back(Fun);
  VtxLocalId.push_back(dfi::LocalVertexId(FS.Vertices.size()));
  VtxEdgeBegin.push_back(0);
  VtxEdgeEnd.push_back(0);
  FS.Vertices.push_back(Vtx);
  if (IsExitVtx) {
    IsExit.insert(Vtx);
  }

  FS.Worklist.push_back(Vtx);
  markDirty(Fun);
  return Vtx;
}

bool DFIExplodedSupergraph::addSeed(dfi::VertexId Vtx) {
  if (!addRoot(Vtx)) {
    return false;
  }
  SeedVertices.push_back(Vtx);
  return true;
}

psr::dfi::EntryId DFIExplodedSupergraph::getOrCreateEntry(dfi::VertexId Vtx) {
  auto [It, Inserted] =
      EntryOf.try_emplace(Vtx, dfi::EntryId(EntryVertex.size()));
  auto Entry = It->second;
  if (Inserted) {
    EntryVertex.push_back(Vtx);
    EntryCallers.emplace_back();
    SummaryExits.emplace_back();
    FunState[VtxFun[Vtx]].Entries.push_back(Entry);
    addRoot(Vtx);
  }
  return Entry;
}

llvm::ArrayRef<psr::dfi::VertexId>
DFIExplodedSupergraph::takeWorklist(FunctionId Fun) {
  Batch.clear();
  std::swap(Batch, FunState[Fun].Worklist);
  llvm::sort(Batch, [this](dfi::VertexId Lhs, dfi::VertexId Rhs) {
    return VtxInst[Lhs] < VtxInst[Rhs];
  });
  return Batch;
}

bool DFIExplodedSupergraph::addSummaryEdge(dfi::VertexId CallVtx,
                                           dfi::VertexId RetVtx) {
  if (!SummaryEdgeSet.insert({CallVtx, RetVtx}).second) {
    return false;
  }
  auto Caller = VtxFun[CallVtx];
  FunState[Caller].SummaryEdges.emplace_back(CallVtx, RetVtx);
  markDirty(Caller);
  return true;
}

llvm::ArrayRef<psr::dfi::VertexId>
DFIExplodedSupergraph::addCallEdge(dfi::VertexId CallVtx, dfi::EntryId Entry) {
  EntryCallers[Entry].push_back(CallVtx);
  CallEdges.emplace_back(CallVtx, EntryVertex[Entry]);
  return SummaryExits[Entry];
}

psr::CsrGraph<psr::dfi::VertexId>
DFIExplodedSupergraph::buildSupergraph() const {
  assert(VtxInst.size() == VtxFun.size() &&
         "Must not build the supergraph after takeVertexInsts()");
  llvm::SmallVector<std::pair<dfi::VertexId, dfi::VertexId>, 0> GraphEdges;
  GraphEdges.reserve(Edges.size() + SummaryEdgeSet.size() + CallEdges.size());
  for (auto From : iota<dfi::VertexId>(numVertices())) {
    for (auto To : edgesOf(From)) {
      GraphEdges.emplace_back(From, To);
    }
  }
  for (const auto &FS : FunState) {
    GraphEdges.append(FS.SummaryEdges.begin(), FS.SummaryEdges.end());
  }
  GraphEdges.append(CallEdges.begin(), CallEdges.end());
  return CsrGraph<dfi::VertexId>::fromEdges(numVertices(), GraphEdges);
}

void DFIExplodedSupergraph::markDirty(FunctionId Fun) {
  FunState[Fun].Dirty = true;
  PendingSCCs.insert(CGSCCs.SCCOfNode[Fun]);
}

bool DFIExplodedSupergraph::addRoot(dfi::VertexId Vtx) {
  if (!IsRoot.tryInsert(Vtx)) {
    return false;
  }
  auto Fun = VtxFun[Vtx];
  FunState[Fun].Roots.push_back(Vtx);
  markDirty(Fun);
  return true;
}

void DFIExplodedSupergraph::indexFunction(FunctionId Fun) {
  auto &FS = FunState[Fun];
  FS.Dirty = false;

  LocalEdges.clear();
  for (auto Vtx : FS.Vertices) {
    auto From = VtxLocalId[Vtx];
    for (auto To : edgesOf(Vtx)) {
      LocalEdges.emplace_back(From, VtxLocalId[To]);
    }
  }
  for (auto [From, To] : FS.SummaryEdges) {
    LocalEdges.emplace_back(VtxLocalId[From], VtxLocalId[To]);
  }
  LocalGraph.assignEdges(FS.Vertices.size(), LocalEdges);

  LocalRoots.clear();
  for (auto Root : FS.Roots) {
    LocalRoots.push_back(VtxLocalId[Root]);
  }

  LocalBuilder.build(
      LocalGraph, LocalRoots,
      [this, &FS](dfi::LocalVertexId Local) {
        return IsExit.contains(FS.Vertices[Local]);
      },
      LocalIndex);
}

llvm::ArrayRef<psr::dfi::VertexId>
DFIExplodedSupergraph::updateSummaryExits(FunctionId Fun, dfi::EntryId Entry) {
  const auto &FS = FunState[Fun];
  auto &Exits = SummaryExits[Entry];

  NewExits.clear();
  LocalIndex.forEachReachableTarget(
      VtxLocalId[EntryVertex[Entry]], [&](dfi::LocalVertexId Local) {
        auto Exit = FS.Vertices[Local];
        if (!std::binary_search(Exits.begin(), Exits.end(), Exit)) {
          NewExits.push_back(Exit);
        }
      });

  if (!NewExits.empty()) {
    Exits.append(NewExits.begin(), NewExits.end());
    llvm::sort(Exits);
  }
  return NewExits;
}
