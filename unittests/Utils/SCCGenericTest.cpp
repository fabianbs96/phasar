/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and other
 *****************************************************************************/

#include "phasar/Utils/SCCGeneric.h"

#include "gtest/gtest.h"

#include <cstddef>
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

  [[nodiscard]] std::vector<GraphNodeId>
  getEdges(const GraphNodeId Vertex) const {
    return Adj[uint32_t(Vertex)];
  }
  std::vector<std::vector<GraphNodeId>> Adj;
};

TEST(SCCGenericTest, SCCTest) {
  using GraphNodeId = ExampleGraph::GraphNodeId;
  ExampleGraph Graph{{{GraphNodeId(2)},
                      {GraphNodeId(0)},
                      {GraphNodeId(1)},
                      {GraphNodeId(1), GraphNodeId(2)},
                      {GraphNodeId(1)},
                      {GraphNodeId(4), GraphNodeId(6)},
                      {GraphNodeId(4), GraphNodeId(7)},
                      {GraphNodeId(5)}}};

  auto OutputRec = analysis::call_graph::execTarjan(Graph, false);
  auto OutputIt = analysis::call_graph::execTarjan(Graph, true);
  ASSERT_EQ(OutputIt.SCCOfNode.size(), Graph.Adj.size())
      << "Iterative Approach did not reach all nodes\n";
  ASSERT_EQ(OutputRec.SCCOfNode.size(), Graph.Adj.size())
      << "Recursive Approach did not reach all nodes\n";
  EXPECT_EQ(OutputRec.NumSCCs, OutputIt.NumSCCs)
      << "Unequal number of SCC components\n";
  for (size_t ID = 0; ID < Graph.Adj.size(); ID++) {
    EXPECT_EQ(OutputRec.SCCOfNode[ID], OutputIt.SCCOfNode[ID])
        << "SCCs differ at Index: " << std::to_string(ID) << "\n";
  }
}

// main function for the test case
int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
