/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#include "phasar/PhasarLLVM/ControlFlow/TypePropagator.h"

#include "phasar/PhasarLLVM/ControlFlow/TypeAssignmentGraph.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/SCCGeneric.h"
#include "phasar/PhasarLLVM/Utils/Compressor.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/PhasarLLVM/Utils/TypeAssignmentGraph.h"

using namespace psr;
using namespace psr::analysis::call_graph;

static void initialize(TypeAssignment &TA, const TypeAssignmentGraph &TAG,
                       const SCCHolder<TAGNodeId> &SCCs) {
  for (const auto &[Node, Types] : TAG.TypeEntryPoints) {
    auto SCC = SCCs.SCCOfNode[size_t(Node)];
    TA.TypesPerSCC[size_t(SCC)].insert(Types.begin(), Types.end());
  }
}

static void propagate(TypeAssignment &TA, const SCCCallers &Deps,
                      SCCId CurrSCC) {
  const auto &Types = TA.TypesPerSCC[size_t(CurrSCC)];
  if (Types.empty())
    return;

  for (auto Succ : Deps.ChildrenOfSCC[size_t(CurrSCC)]) {
    TA.TypesPerSCC[size_t(Succ)].insert(Types.begin(), Types.end());
  }
}

TypeAssignment analysis::call_graph::propagateTypes(
    const TypeAssignmentGraph &TAG, const SCCHolder &SCCs,
    const SCCCallers &Deps, const SCCOrder &Order) {
  TypeAssignment Ret;
  Ret.TypesPerSCC.resize(SCCs.NumSCCs);

  initialize(Ret, TAG, SCCs);
  for (auto SCC : Order.SCCIds) {
    propagate(Ret, Deps, SCC);
  }

  return Ret;
}

void TypeAssignment::print(llvm::raw_ostream &OS,
                           const TypeAssignmentGraph &TAG,
                           const SCCHolder &SCCs) {
  OS << "digraph TypeAssignment {\n";
  psr::scope_exit CloseBrace = [&OS] { OS << "}\n"; };

  Compressor<const llvm::Value *> Types;
  auto GetOrAddType = [&](const llvm::Value *Ty) {
    auto [Id, Inserted] = Types.insert(Ty);
    if (Inserted) {
      OS << (size_t(Id) + SCCs.NumSCCs) << "[label=\"";
      OS.write_escaped(Ty->getName());
      OS << "\"];\n";
    }
    return Id + SCCs.NumSCCs;
  };

  for (size_t Ctr = 0; Ctr != SCCs.NumSCCs; ++Ctr) {
    OS << "  " << Ctr << "[label=\"";
    for (auto TNId : SCCs.NodesInSCC[Ctr]) {
      auto TN = TAG.Nodes[TNId];
      printNode(OS, TN);
      OS << "\\n";
    }
    OS << "\"];\n";

    for (const auto *Ty : TypesPerSCC[Ctr]) {
      auto TyId = GetOrAddType(Ty);
      OS << Ctr << "->" << TyId << ";\n";
    }
  }
}
