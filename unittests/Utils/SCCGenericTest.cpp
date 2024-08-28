/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

// #include "phasar/PhasarLLVM/DataFlow/IfdsIde/SCCGeneric.h"
#include "SCCGeneric.h"

// #include "phasar/PhasarLLVM/Utils/TypeAssignmentGraph.h"

#include <iostream>

// #include "phasar/PhasarLLVM/Utils/Compressor.h"
// #include "phasar/PhasarLLVM/Utils/TypeAssignmentGraph.h"

#include "llvm/ADT/SmallBitVector.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

using namespace psr;

using SCCId = analysis::call_graph::SCCId;

class ExampleGraph {
public:
  ExampleGraph() = default;

  std::vector<analysis::call_graph::GraphNodeId>
  getEdges(const analysis::call_graph::GraphNodeId ID) const {
    return Adj[uint32_t(ID)];
  }
  std::vector<std::vector<analysis::call_graph::GraphNodeId>> Adj;
};

int main() {
  ExampleGraph Gr;
  std::vector<std::vector<analysis::call_graph::GraphNodeId>> list = {
      {analysis::call_graph::GraphNodeId(2)},
      {analysis::call_graph::GraphNodeId(0)},
      {analysis::call_graph::GraphNodeId(1)},
      {analysis::call_graph::GraphNodeId(1),
       analysis::call_graph::GraphNodeId(2)},
      {analysis::call_graph::GraphNodeId(1)},
      {analysis::call_graph::GraphNodeId(4),
       analysis::call_graph::GraphNodeId(6)},
      {analysis::call_graph::GraphNodeId(4),
       analysis::call_graph::GraphNodeId(7)},
      {analysis::call_graph::GraphNodeId(5)}};

  auto Output = analysis::call_graph::execTarjan(Gr, false);
  std::cout << Output.NumSCCs;
  auto Out = analysis::call_graph::execTarjan(Gr, true);
  std::cout << Out.NumSCCs;
}
