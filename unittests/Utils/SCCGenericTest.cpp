/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#include "phasar/PhasarLLVM/DataFlow/IfdsIde/SCCGeneric.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <string>

//===----------------------------------------------------------------------===//
// Unit tests for the Igeneric SCC algorithm

using namespace psr;

using SCCId = analysis::call_graph::SCCId;
enum class [[clang::enum_extensibility(open)]] NodeId : uint32_t{};

class ExampleGraph {
public:
  using GraphNodeId = NodeId;

  ExampleGraph() = default;

  [[nodiscard]] std::vector<GraphNodeId> getEdges(const GraphNodeId ID) const {
    return Adj[uint32_t(ID)];
  }
  std::vector<std::vector<GraphNodeId>> Adj;
};

TEST(SCCGenericTest, SCCTest) {
  ExampleGraph Graph;
  std::vector<std::vector<ExampleGraph::GraphNodeId>> list = {
      {ExampleGraph::GraphNodeId(2)},
      {ExampleGraph::GraphNodeId(0)},
      {ExampleGraph::GraphNodeId(1)},
      {ExampleGraph::GraphNodeId(1), ExampleGraph::GraphNodeId(2)},
      {ExampleGraph::GraphNodeId(1)},
      {ExampleGraph::GraphNodeId(4), ExampleGraph::GraphNodeId(6)},
      {ExampleGraph::GraphNodeId(4), ExampleGraph::GraphNodeId(7)},
      {ExampleGraph::GraphNodeId(5)}};

  auto OutputRec = analysis::call_graph::execTarjan(Graph, false);
  auto OutputIt = analysis::call_graph::execTarjan(Graph, true);
  ASSERT_EQ(OutputRec.NumSCCs, OutputIt.NumSCCs)
      << "Unequal number of SCC components\n"
      << "Abort Test\n";
  for (int ID = 0; ID < Graph.Adj.size(); ID++) {
    EXPECT_EQ(OutputRec.SCCOfNode[ID], OutputIt.SCCOfNode[ID])
        << "SCCs differ at Index: " << std::to_string(ID) << "\n";
  }
}

// main function for the test case
int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
