#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"

#include "phasar/Config/Configuration.h"
#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include "llvm/Support/raw_ostream.h"

#include "TestConfig.h"
#include "gtest/gtest.h"

#include <string>
#include <vector>

#include <boost/container/flat_set.hpp>

using namespace std;
using namespace psr;

template <typename T> static auto makeSet(T &&Vec) {
  using value_type = std::decay_t<decltype(*Vec.begin())>;
  return std::set<value_type>{Vec.begin(), Vec.end()};
}

TEST(LLVMBasedICFGTest, StaticCallSite_1) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/static_callsite_1.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *F = IRDB.getFunctionDefinition("main");
  const llvm::Function *Foo = IRDB.getFunctionDefinition("foo");
  ASSERT_TRUE(F);
  ASSERT_TRUE(Foo);
  // iterate all instructions
  for (const auto &BB : *F) {
    for (const auto &I : BB) {
      // inspect call-sites
      if (llvm::isa<llvm::CallInst>(&I) || llvm::isa<llvm::InvokeInst>(&I)) {
        ASSERT_FALSE(ICFG.isVirtualFunctionCall(&I));
      }
    }
  }
}

TEST(LLVMBasedICFGTest, StaticCallSite_2a) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/static_callsite_2.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT,
                     Soundness::Soundy, false);
  const llvm::Function *F = IRDB.getFunctionDefinition("main");
  const llvm::Function *FOO = IRDB.getFunctionDefinition("foo");
  const llvm::Function *BAR = IRDB.getFunctionDefinition("bar");
  ASSERT_TRUE(F);
  ASSERT_TRUE(FOO);
  ASSERT_TRUE(BAR);

  set<const llvm::Function *> FunctionSet;
  FunctionSet.insert(F);
  FunctionSet.insert(FOO);
  FunctionSet.insert(BAR);

  set<const llvm::Function *> FunSet = makeSet(ICFG.getAllFunctions());
  ASSERT_EQ(FunctionSet, FunSet);

  set<const llvm::Instruction *> CallsFromWithin =
      makeSet(ICFG.getCallsFromWithin(F));
  ASSERT_EQ(CallsFromWithin.size(), 2U);
}

TEST(LLVMBasedICFGTest, StaticCallSite_2b) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/static_callsite_2.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *F = IRDB.getFunctionDefinition("main");
  const llvm::Function *FOO = IRDB.getFunctionDefinition("foo");
  const llvm::Function *BAR = IRDB.getFunctionDefinition("bar");
  const llvm::Function *CTOR =
      IRDB.getFunctionDefinition("__psrCRuntimeGlobalCtorsModel");
  const llvm::Function *DTOR =
      IRDB.getFunctionDefinition("__psrCRuntimeGlobalDtorsModel");
  ASSERT_TRUE(F);
  ASSERT_TRUE(FOO);
  ASSERT_TRUE(BAR);
  ASSERT_TRUE(CTOR);
  ASSERT_TRUE(DTOR);

  set<const llvm::Function *> FunctionSet;
  FunctionSet.insert(F);
  FunctionSet.insert(FOO);
  FunctionSet.insert(BAR);
  FunctionSet.insert(CTOR);
  FunctionSet.insert(DTOR);

  auto FunSet = makeSet(ICFG.getAllFunctions());
  ASSERT_EQ(FunctionSet, FunSet);

  set<const llvm::Instruction *> CallsFromWithin =
      makeSet(ICFG.getCallsFromWithin(F));
  ASSERT_EQ(CallsFromWithin.size(), 2U);
}

TEST(LLVMBasedICFGTest, VirtualCallSite_1) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/virtual_call_1.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *F = IRDB.getFunctionDefinition("main");
  const llvm::Function *FooA = IRDB.getFunctionDefinition("_ZN1A3fooEv");
  ASSERT_TRUE(F);
  ASSERT_TRUE(FooA);

  for (const auto &BB : *F) {
    for (const auto &I : BB) {
      if (llvm::isa<llvm::CallInst>(&I) || llvm::isa<llvm::InvokeInst>(&I)) {
        ASSERT_FALSE(ICFG.isIndirectFunctionCall(&I));
      }
    }
  }
}

TEST(LLVMBasedICFGTest, FunctionPointer_1) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/function_pointer_1.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *F = IRDB.getFunctionDefinition("main");
  const llvm::Function *Foo = IRDB.getFunctionDefinition("fptr");
  ASSERT_TRUE(F);
  ASSERT_FALSE(Foo);
  for (const auto &BB : *F) {
    for (const auto &I : BB) {
      if (llvm::isa<llvm::CallInst>(&I) || llvm::isa<llvm::InvokeInst>(&I)) {
        ASSERT_TRUE(ICFG.getFunctionOf(&I));
      }
    }
  }
}

TEST(LLVMBasedICFGTest, StaticCallSite_3) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/static_callsite_3.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *Factorial = IRDB.getFunctionDefinition("factorial");
  ASSERT_TRUE(Factorial);
  for (const auto &BB : *Factorial) {
    for (const auto &I : BB) {
      set<const llvm::Instruction *> CallsFromWithin =
          makeSet(ICFG.getCallsFromWithin(ICFG.getFunctionOf(&I)));
      for (const llvm::Instruction *Inst : CallsFromWithin) {
        auto MethodName = ICFG.getFunctionName(ICFG.getFunctionOf(Inst));
        ASSERT_EQ(MethodName, "factorial");
      }
    }
  }
}

TEST(LLVMBasedICFGTest, StaticCallSite_4) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/static_callsite_4.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *F = IRDB.getFunctionDefinition("main");
  ASSERT_TRUE(F);

  set<const llvm::Function *> CalleesOfCallAt;
  set<const llvm::Function *> CalleesOfCallAtInside;
  int CountFunc = 0;

  for (const auto &BB : *F) {
    for (const auto &I : BB) {
      CalleesOfCallAt = makeSet(ICFG.getCalleesOfCallAt(&I));
      for (const llvm::Function *Func : CalleesOfCallAt) {
        for (const auto &BB2 : *Func) {
          for (const auto &I2 : BB2) {
            if (llvm::isa<llvm::CallInst>(&I2)) {
              CalleesOfCallAtInside = makeSet(ICFG.getCalleesOfCallAt(&I2));
              CountFunc = CountFunc + CalleesOfCallAtInside.size();
              ASSERT_FALSE(ICFG.isVirtualFunctionCall(&I));
              ASSERT_FALSE(ICFG.isVirtualFunctionCall(&I2));
            }
          }
        }
      }
    }
  }
  ASSERT_EQ(CountFunc, 2);
}

TEST(LLVMBasedICFGTest, StaticCallSite_5) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/static_callsite_5.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *F = IRDB.getFunctionDefinition("main");
  const llvm::Function *Foo =
      IRDB.getFunctionDefinition("_ZN3Foo10getNumFoosEv");
  ASSERT_TRUE(F);
  ASSERT_TRUE(Foo);

  for (const auto &BB : *F) {
    for (const auto &I : BB) {
      if (ICFG.isCallSite(&I)) {
        set<const llvm::Instruction *> CallsFromWithin =
            makeSet(ICFG.getCallsFromWithin(ICFG.getFunctionOf(&I)));
        ASSERT_EQ(CallsFromWithin.size(), 1U);
      }
    }
  }
}

TEST(LLVMBasedICFGTest, StaticCallSite_6) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/static_callsite_6.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *F = IRDB.getFunctionDefinition("main");
  const llvm::Function *FooF = IRDB.getFunctionDefinition("_ZN3Foo1fEv");
  ASSERT_TRUE(F);
  ASSERT_TRUE(FooF);

  const llvm::Instruction *I = getNthInstruction(F, 1);
  if (ICFG.isCallSite(I) || llvm::isa<llvm::InvokeInst>(I)) {
    set<const llvm::Instruction *> StartPoints =
        makeSet(ICFG.getStartPointsOf(FooF));
    set<const llvm::Instruction *> CallsFromWithin = makeSet(
        ICFG.getCallsFromWithin(ICFG.getFunctionOf(getNthInstruction(F, 2))));

    ASSERT_EQ(StartPoints.size(), 1U);
    ASSERT_TRUE(StartPoints.count(I));
    ASSERT_EQ(CallsFromWithin.size(), 2U);
    ASSERT_TRUE(CallsFromWithin.count(getNthInstruction(F, 2)));
    ASSERT_TRUE(CallsFromWithin.count(getNthInstruction(FooF, 4)));
  }
}

TEST(LLVMBasedICFGTest, StaticCallSite_7) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/static_callsite_7.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *Main = IRDB.getFunctionDefinition("main");
  const llvm::Function *FooF = IRDB.getFunctionDefinition("_ZN3Foo1fEv");
  const llvm::Function *F = IRDB.getFunctionDefinition("_Z1fv");
  ASSERT_TRUE(Main);
  ASSERT_TRUE(FooF);
  ASSERT_TRUE(F);

  const llvm::Instruction *I = getNthInstruction(FooF, 4);
  const llvm::Instruction *LastInst =
      getLastInstructionOf(IRDB.getFunctionDefinition("_ZN3Foo1fEv"));
  set<const llvm::Function *> AllMethods = makeSet(ICFG.getAllFunctions());
  ASSERT_EQ(LastInst, I);
  ASSERT_EQ(AllMethods.size(), 5U);
  ASSERT_TRUE(AllMethods.count(Main));
  ASSERT_TRUE(AllMethods.count(FooF));
  ASSERT_TRUE(AllMethods.count(F));
}

TEST(LLVMBasedICFGTest, StaticCallSite_8) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/static_callsite_8.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT);
  const llvm::Function *F = IRDB.getFunctionDefinition("main");
  const llvm::Function *FooF = IRDB.getFunctionDefinition("_ZN4Foo21fEv");
  ASSERT_TRUE(F);
  ASSERT_TRUE(FooF);

  // std::vector<const llvm::Instruction *> Insts =
  //     ICFG.getAllInstructionsOf(FooF);
  // std::vector<const llvm::Instruction *> Insts1 =
  //     ICFG.getAllInstructionsOf(IRDB.getFunctionDefinition("_ZN4Foo21fEv"));
  // ASSERT_EQ(Insts.size(), Insts1.size());

  set<const llvm::Function *> FunSet = makeSet(ICFG.getAllFunctions());
  ASSERT_EQ(FunSet.size(), 5U);

  const llvm::Instruction *I = getNthInstruction(F, 1);
  ASSERT_TRUE(ICFG.isStartPoint(I));
}

TEST(LLVMBasedICFGTest, GlobalCtorDtor_1) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/global_ctor_dtor_1.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT,
                     Soundness::Soundy, true);

  auto *GlobCtorFn = IRDB.getFunction(LLVMBasedICFG::GlobalCRuntimeModelName);

  ASSERT_TRUE(GlobCtorFn);

  // GlobCtorFn->print(llvm::outs());

  const llvm::Function *Main = IRDB.getFunctionDefinition("main");
  const llvm::Function *BeforeMain =
      IRDB.getFunctionDefinition("_Z11before_mainv");

  ASSERT_TRUE(Main);
  ASSERT_TRUE(BeforeMain);

  auto VertFuns = makeSet(ICFG.getAllVertexFunctions());

  ASSERT_TRUE(VertFuns.count(Main));
  ASSERT_TRUE(VertFuns.count(BeforeMain));
}

TEST(LLVMBasedICFGTest, GlobalCtorDtor_2) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/global_ctor_dtor_2.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT,
                     Soundness::Soundy, true);
  const llvm::Function *Main = IRDB.getFunctionDefinition("main");
  const llvm::Function *BeforeMain =
      IRDB.getFunctionDefinition("_Z11before_mainv");
  const llvm::Function *AfterMain =
      IRDB.getFunctionDefinition("_Z10after_mainv");

  ASSERT_TRUE(Main);
  ASSERT_TRUE(BeforeMain);
  ASSERT_TRUE(AfterMain);

  auto VertFuns = makeSet(ICFG.getAllVertexFunctions());

  ASSERT_TRUE(VertFuns.count(Main));
  ASSERT_TRUE(VertFuns.count(BeforeMain));
  ASSERT_TRUE(VertFuns.count(AfterMain));
}

TEST(LLVMBasedICFGTest, GlobalCtorDtor_3) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/global_ctor_dtor_3.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT,
                     Soundness::Soundy, true);
  const llvm::Function *Main = IRDB.getFunctionDefinition("main");
  const llvm::Function *Ctor = IRDB.getFunctionDefinition("_ZN1SC2Ei");
  const llvm::Function *Dtor = IRDB.getFunctionDefinition("_ZN1SD2Ev");

  ASSERT_TRUE(Main);
  ASSERT_TRUE(Ctor);
  ASSERT_TRUE(Dtor);

  auto VertFuns = makeSet(ICFG.getAllVertexFunctions());

  ASSERT_TRUE(VertFuns.count(Ctor));
  ASSERT_TRUE(VertFuns.count(Dtor));
}

TEST(LLVMBasedICFGTest, GlobalCtorDtor_4) {
  LLVMProjectIRDB IRDB({"llvm_test_code/call_graphs/global_ctor_dtor_4.ll"});
  LLVMTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::CHA, {"main"}, &TH, &PT,
                     Soundness::Soundy, true);
  const llvm::Function *Main = IRDB.getFunctionDefinition("main");
  const llvm::Function *Ctor = IRDB.getFunctionDefinition("_ZN1SC2Ei");
  const llvm::Function *Dtor = IRDB.getFunctionDefinition("_ZN1SD2Ev");
  const llvm::Function *BeforeMain =
      IRDB.getFunctionDefinition("_Z11before_mainv");
  const llvm::Function *AfterMain =
      IRDB.getFunctionDefinition("_Z10after_mainv");

  ASSERT_TRUE(Main);
  ASSERT_TRUE(Ctor);
  ASSERT_TRUE(Dtor);
  ASSERT_TRUE(BeforeMain);
  ASSERT_TRUE(AfterMain);

  auto VertFuns = makeSet(ICFG.getAllVertexFunctions());

  ASSERT_TRUE(VertFuns.count(Ctor));
  ASSERT_TRUE(VertFuns.count(Dtor));
  ASSERT_TRUE(VertFuns.count(Main));
  ASSERT_TRUE(VertFuns.count(BeforeMain));
  ASSERT_TRUE(VertFuns.count(AfterMain));
}

TEST(LLVMBasedICFGTest, RuntimeEdges_1) {
  psr::ValueAnnotationPass::resetValueID();
  psr::FunctionAnnotationPass::resetFunctionID();
  ProjectIRDB IRDB({"llvm_test_code/call_graphs/runtime_edges_1.ll"},
                   IRDBOptions::WPA);
  LLVMTypeHierarchy TH(IRDB);
  LLVMPointsToSet PT(IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::NORESOLVE, {"main"}, &TH,
                     &PT, Soundness::Unsound, true);

  // llvm::errs() << *IRDB.getWPAModule() << '\n';

  const llvm::Function *Main = IRDB.getFunctionDefinition("main");
  const llvm::Function *Foo = IRDB.getFunctionDefinition("_Z3foov");
  const llvm::Function *Bar = IRDB.getFunctionDefinition("_Z3barv");

  ASSERT_TRUE(Main);
  ASSERT_TRUE(Foo);

  auto VertFunsBefore = ICFG.getAllVertexFunctions();

  ASSERT_TRUE(llvm::find(VertFunsBefore, Bar) == std::end(VertFunsBefore));
  auto TotalEdgesBefore = ICFG.getNumOfEdges();

  // std::vector<std::pair<unsigned, unsigned>> RuntimeEdges = {{10, 1}};
  auto ICFGChanged = // ICFG.addRuntimeEdges(RuntimeEdges);
      ICFG.addEdgeToICFG(IRDB.getInstruction(10), IRDB.getFunctionById(1));

  ASSERT_TRUE(ICFGChanged);
  auto VertFunsAfter = ICFG.getAllVertexFunctions();
  ASSERT_TRUE(llvm::find(VertFunsAfter, Bar) != std::end(VertFunsAfter));
  ASSERT_TRUE(ICFG.getNumOfEdges() == TotalEdgesBefore + 1);
  ASSERT_TRUE(llvm::find(ICFG.getCallersOf(Bar), IRDB.getInstruction(10)) !=
              ICFG.getCallersOf(Bar).end());
  ASSERT_TRUE(ICFG.getTotalRuntimeEdgesAdded() == 1);
}

TEST(LLVMBasedICFGTest, RuntimeEdges_2) {
  psr::ValueAnnotationPass::resetValueID();
  psr::FunctionAnnotationPass::resetFunctionID();
  ProjectIRDB IRDB({"llvm_test_code/call_graphs/runtime_edges_1.ll"},
                   IRDBOptions::WPA);
  LLVMTypeHierarchy TH(IRDB);
  LLVMPointsToSet PT(IRDB);
  LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::OTF, {"main"}, &TH, &PT,
                     Soundness::Unsound, true);
  const llvm::Function *Main = IRDB.getFunctionDefinition("main");
  const llvm::Function *Foo = IRDB.getFunctionDefinition("_Z3foov");
  const llvm::Function *Bar = IRDB.getFunctionDefinition("_Z3barv");

  ASSERT_TRUE(Main);
  ASSERT_TRUE(Foo);

  auto VertFunsBefore = ICFG.getAllVertexFunctions();

  ASSERT_TRUE(llvm::find(VertFunsBefore, Bar) == std::end(VertFunsBefore));
  auto TotalEdgesBefore = ICFG.getNumOfEdges();

  // std::vector<std::pair<unsigned, unsigned>> RuntimeEdges = {{10, 0}};
  auto ICFGChanged = // ICFG.addRuntimeEdges(RuntimeEdges);
      ICFG.addEdgeToICFG(IRDB.getInstruction(10), IRDB.getFunctionById(0));

  ASSERT_FALSE(ICFGChanged);
  auto VertFunsAfter = ICFG.getAllVertexFunctions();
  ASSERT_TRUE(llvm::find(VertFunsAfter, Bar) == std::end(VertFunsAfter));
  ASSERT_TRUE(ICFG.getTotalRuntimeEdgesAdded() == 0);
}
