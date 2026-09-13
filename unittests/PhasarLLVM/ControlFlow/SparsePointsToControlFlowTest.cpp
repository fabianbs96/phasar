#include "phasar/PhasarLLVM/ControlFlow/SparsePointsToControlFlow.h"

#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseCFGCache.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToInfo.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Casting.h"

#include "TestConfig.h"
#include "gtest/gtest.h"

using namespace psr;

namespace {
using Policy = SparsePointsToControlFlow<LLVMPointsToIteratorRef>;

llvm::DenseSet<Policy::o_t> globalObjectsOf(const LLVMProjectIRDB &IRDB,
                                            LLVMPointsToIteratorRef PT) {
  llvm::DenseSet<Policy::o_t> Globals;
  for (const auto &GV : IRDB.getModule()->global_values()) {
    Globals.insert(PT.asAbstractObject(&GV));
  }
  return Globals;
}
} // namespace

TEST(SparsePointsToControlFlowTest, DefiningInstIsAlwaysKept) {
  LLVMProjectIRDB IRDB(unittest::PathToLLTestFiles +
                       "pointers/basic_01_c_dbg.ll");
  LLVMAliasSet AS(&IRDB);
  LLVMPointsToIteratorRef PT = &AS;
  auto KnownGlobals = globalObjectsOf(IRDB, PT);

  const auto *V = IRDB.getInstruction(5);
  ASSERT_TRUE(V && V->getType()->isPointerTy());

  EXPECT_TRUE(
      Policy::shouldKeepInst(V, PT.asAbstractObject(V), PT, KnownGlobals));
}

TEST(SparsePointsToControlFlowTest, FactAsValueOrObjectAgree) {
  LLVMProjectIRDB IRDB(unittest::PathToLLTestFiles +
                       "pointers/basic_01_c_dbg.ll");
  LLVMAliasSet AS(&IRDB);
  LLVMPointsToIteratorRef PT = &AS;
  auto KnownGlobals = globalObjectsOf(IRDB, PT);

  const auto *V = IRDB.getInstruction(5);
  ASSERT_TRUE(V && V->getType()->isPointerTy());
  const auto *Entry = &V->getFunction()->getEntryBlock().front();

  // Same fact, once passed as v_t, once pre-converted to o_t.
  const auto *NextFromValue =
      Policy::advanceToNextUser(Entry, V, PT, KnownGlobals);
  const auto *NextFromObject = Policy::advanceToNextUser(
      Entry, PT.asAbstractObject(V), PT, KnownGlobals);
  EXPECT_EQ(NextFromValue, NextFromObject);
}

TEST(SparsePointsToControlFlowTest, GlobalRetainedAtCallSite) {
  // main() never touches g1 at all (not even indirectly through foo()), yet
  // the call must still be conservatively retained for fact g1: the
  // retention rule cannot know that foo() doesn't touch it.
  auto IRDB = LLVMProjectIRDB::loadOrExit(
      unittest::PathToLLTestFiles + "linear_constant/global_11_cpp_dbg.ll");
  LLVMAliasSet AS(&IRDB);
  LLVMPointsToIteratorRef PT = &AS;

  const auto *G1 = IRDB.getGlobalVariable("g1");
  ASSERT_TRUE(G1);
  const auto *Main = IRDB.getFunctionDefinition("main");
  ASSERT_TRUE(Main);
  const auto *Entry = &Main->getEntryBlock().front();

  auto Insts = llvm::instructions(Main);
  auto CallIt = llvm::find_if(Insts, [](const llvm::Instruction &I) {
    // Exclude debug/lifetime intrinsic calls, which shouldKeepInst treats
    // as no-ops -- we want the real call to foo().
    return llvm::isa<llvm::CallBase>(I) && !llvm::isa<llvm::IntrinsicInst>(I);
  });
  ASSERT_NE(CallIt, Insts.end());
  const auto *Call = &*CallIt;

  LLVMBasedCFG CFG;
  SparseCFGCache<Policy, LLVMPointsToIteratorRef> Cache;
  const auto &SCFG = Cache.getOrCreate(CFG, Main, PT.asAbstractObject(G1), PT);
  EXPECT_EQ(Call, SCFG.nextUserOrNull(Entry));
}

int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
