/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFG.h"

#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/StrongTypeDef.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>

using namespace psr;

SVFGNodeId SVFG::addNode(SVFGNode N) {
  auto Id = SVFGNodeId(Nodes.size());
  if (N.IRValue) {
    ValueToNodes[N.IRValue].push_back(Id);
  }
  Nodes.emplace_back(N);
  Fwd.emplace_back();
  Rev.emplace_back();
  return Id;
}

void SVFG::addEdge(SVFGNodeId From, SVFGNodeId To, SVFGEdgeKind Kind) {
  Fwd[From].push_back({To, Kind});
}

void SVFG::finalize() {
  for (auto [Id, Edges] : Fwd.enumerate()) {
    llvm::sort(Edges);
    Edges.erase(std::unique(Edges.begin(), Edges.end()), Edges.end());
    for (const auto &E : Edges) {
      Rev[E.Target].push_back({Id, E.Kind});
    }
  }
  for (auto &Edges : Rev) {
    llvm::sort(Edges);
    Edges.erase(std::unique(Edges.begin(), Edges.end()), Edges.end());
  }
}

llvm::ArrayRef<SVFGEdge> SVFG::succEdges(SVFGNodeId N) const noexcept {
  return Fwd[N];
}

llvm::ArrayRef<SVFGEdge> SVFG::predEdges(SVFGNodeId N) const noexcept {
  return Rev[N];
}

const SVFGNode &SVFG::node(SVFGNodeId N) const noexcept { return Nodes[N]; }

llvm::ArrayRef<SVFGNodeId> SVFG::nodesFor(const llvm::Value *V) const noexcept {
  auto It = ValueToNodes.find(V);
  if (It == ValueToNodes.end()) {
    return {};
  }
  return It->second;
}

static llvm::StringRef edgeColor(SVFGEdgeKind K) noexcept {
  switch (K) {
  case SVFGEdgeKind::Direct:
    return "black";
  case SVFGEdgeKind::Indirect:
    return "orange";
  case SVFGEdgeKind::Call:
    return "blue";
  case SVFGEdgeKind::Return:
    return "red";
  }
  return "black";
}

void SVFG::printAsDot(llvm::raw_ostream &OS) const {
  OS << "digraph SVFG {\n";
  OS << "  node [shape=rectangle];\n";

  for (auto [Id, N] : Nodes.enumerate()) {
    OS << "  N_" << to_underlying(Id) << " [label=\""
       << svfgNodeKindName(N.Kind) << ": ";
    if (N.IRValue) {
      OS << llvmIRToShortString(N.IRValue);
    }
    OS << "\"];\n";
  }

  for (auto [Id, Edges] : Fwd.enumerate()) {
    for (const auto &E : Edges) {
      OS << "  N_" << to_underlying(Id) << " -> N_" << to_underlying(E.Target)
         << " [label=\"" << svfgEdgeKindName(E.Kind) << "\" color=\""
         << edgeColor(E.Kind) << "\"];\n";
    }
  }

  OS << "}\n";
}

std::string SVFG::exportAsDot() const {
  std::string Result;
  llvm::raw_string_ostream OS(Result);
  printAsDot(OS);
  return Result;
}
