/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFG.h"

#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFGBuilder.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/Utils/StrongTypeDef.h"

#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/raw_ostream.h"

#include "SrcCodeLocationEntry.h"
#include "TestConfig.h"
#include "gtest/gtest.h"

using namespace psr;
using namespace psr::unittest;

// ---- Helpers ----------------------------------------------------------------

static constexpr SVFGNodeId InvalidNodeId =
    llvm::DenseMapInfo<SVFGNodeId>::getEmptyKey();

static bool hasEdge(const SVFG &G, SVFGNodeId From, SVFGNodeId To,
                    SVFGEdgeKind Kind) {
  return llvm::any_of(G.succEdges(From), [&](const SVFGEdge &E) {
    return E.Target == To && E.Kind == Kind;
  });
}

static SVFGNodeId findNodeOfKind(const SVFG &G,
                                 llvm::ArrayRef<SVFGNodeId> Nodes,
                                 SVFGNodeKind Kind) {
  for (auto Id : Nodes) {
    if (G.node(Id).Kind == Kind) {
      return Id;
    }
  }
  return InvalidNodeId;
}

// ---- Fixture ----------------------------------------------------------------

// static_callsite_3.c:
//   line 3: int factorial(unsigned int n){return n==0?1:factorial(n-1);}
//   line 5: int main(int argc,char**argv){int f=factorial(argc);return 0;}
//
// Relevant debug locations (from the IR):
//   load %0 in factorial   → line 3, col 42
//   icmp %cmp in factorial → line 3, col 44
//   ret in factorial       → RetStmt{"factorial"}
//   call factorial in main → line 5, col 43

static constexpr auto Callsite3Dbg =
    PHASAR_BUILD_SUBFOLDER("call_graphs/static_callsite_3_c_dbg.ll");

struct SVFGFixture : public ::testing::Test {
  LLVMProjectIRDB IRDB{Callsite3Dbg};
  DIBasedTypeHierarchy TH{IRDB};
  LLVMAliasSet PT{&IRDB, false};
  LLVMBasedICFG ICFG{&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT};
  SVFG G{SVFGBuilder(ICFG, &PT).build()};
};

// ---- Tests ------------------------------------------------------------------

TEST_F(SVFGFixture, DirectEdge_LoadToCmp) {
  const auto *Load0 = testingLocInIR(LineColFun{3, 42, "factorial"}, IRDB);
  const auto *Cmp = testingLocInIR(LineColFun{3, 44, "factorial"}, IRDB);
  ASSERT_NE(Load0, nullptr);
  ASSERT_NE(Cmp, nullptr);

  auto Load0Nodes = G.nodesFor(Load0);
  auto CmpNodes = G.nodesFor(Cmp);
  ASSERT_EQ(Load0Nodes.size(), 1U);
  ASSERT_EQ(CmpNodes.size(), 1U);

  EXPECT_EQ(G.node(Load0Nodes[0]).Kind, SVFGNodeKind::Load);
  EXPECT_EQ(G.node(CmpNodes[0]).Kind, SVFGNodeKind::Copy);
  EXPECT_TRUE(hasEdge(G, Load0Nodes[0], CmpNodes[0], SVFGEdgeKind::Direct))
      << "Expected Direct edge from load %0 to icmp %cmp in factorial";

  G.printAsDot(llvm::outs());
}

TEST_F(SVFGFixture, FormalRetNode_HasKind) {
  const auto *RetInst = testingLocInIR(RetStmt{"factorial"}, IRDB);
  ASSERT_NE(RetInst, nullptr);

  auto FRNodes = G.nodesFor(RetInst);
  ASSERT_EQ(FRNodes.size(), 1U);
  EXPECT_EQ(G.node(FRNodes[0]).Kind, SVFGNodeKind::FormalRet);
}

TEST_F(SVFGFixture, CallEdge_FormalParamHasCallPred) {
  // %n in factorial
  const auto *ParamN = testingLocInIR(ArgInFun{0, "factorial"}, IRDB);
  ASSERT_NE(ParamN, nullptr);

  auto FPNodes = G.nodesFor(ParamN);
  ASSERT_EQ(FPNodes.size(), 1U);
  EXPECT_EQ(G.node(FPNodes[0]).Kind, SVFGNodeKind::FormalParam);

  EXPECT_TRUE(llvm::any_of(G.predEdges(FPNodes[0]), [](const SVFGEdge &E) {
    return E.Kind == SVFGEdgeKind::Call;
  })) << "Expected Call-edge predecessor of FormalParam(%n)";
}

TEST_F(SVFGFixture, ReturnEdge_FormalRetToActualRet) {
  const auto *RetInst = testingLocInIR(RetStmt{"factorial"}, IRDB);
  const auto *CallInMain = testingLocInIR(LineColFun{5, 43, "main"}, IRDB);
  ASSERT_NE(RetInst, nullptr);
  ASSERT_NE(CallInMain, nullptr);

  auto FRNodes = G.nodesFor(RetInst);
  ASSERT_EQ(FRNodes.size(), 1U);
  auto FRId = FRNodes[0];

  auto ARNodes = G.nodesFor(CallInMain);
  auto ARId = findNodeOfKind(G, ARNodes, SVFGNodeKind::ActualRet);
  ASSERT_NE(ARId, InvalidNodeId)
      << "Expected ActualRet node for the call to factorial in main";

  EXPECT_TRUE(hasEdge(G, FRId, ARId, SVFGEdgeKind::Return))
      << "Expected Return edge: FormalRet(factorial) -> ActualRet(call@main)";
}

TEST_F(SVFGFixture, Bidirectional_AllForwardEdgesHaveReverseCounterpart) {
  for (auto V : GraphTraits<SVFG>::vertices(G)) {
    for (const auto &E : G.succEdges(V)) {
      EXPECT_TRUE(llvm::any_of(G.predEdges(E.Target),
                               [&](const SVFGEdge &PE) {
                                 return PE.Target == V && PE.Kind == E.Kind;
                               }))
          << "Missing reverse edge for N_" << to_underlying(V) << " -> N_"
          << to_underlying(E.Target) << " (" << svfgEdgeKindName(E.Kind).str()
          << ")";
    }
  }
}

TEST_F(SVFGFixture, ValueLookup_NodeForIRValueRoundtrips) {
  for (auto V : GraphTraits<SVFG>::vertices(G)) {
    const auto &N = G.node(V);
    if (!N.IRValue) {
      continue;
    }
    auto Nodes = G.nodesFor(N.IRValue);
    EXPECT_FALSE(Nodes.empty());
    EXPECT_TRUE(llvm::is_contained(Nodes, V));
  }
}

TEST_F(SVFGFixture, DotExport_ContainsExpectedKeywords) {
  std::string Dot = G.exportAsDot();
  EXPECT_FALSE(Dot.empty());
  EXPECT_NE(Dot.find("digraph"), std::string::npos);
  EXPECT_NE(Dot.find("FormalParam"), std::string::npos);
  EXPECT_NE(Dot.find("Return"), std::string::npos);
}

int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
