#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseCFGCache.h"
#include "phasar/PhasarLLVM/ControlFlow/SparsePointsToControlFlow.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Pointer/SVF/SVFPointsToSet.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Casting.h"

#include "TestConfig.h"
#include "gtest/gtest.h"

using namespace psr;

namespace {
using Policy = SparsePointsToControlFlow<SVFBasedPointsToIteratorRef>;
static_assert(!std::is_same_v<Policy::o_t, Policy::v_t>,
              "This test is specifically about o_t != v_t");

llvm::DenseSet<Policy::o_t> globalObjectsOf(const LLVMProjectIRDB &IRDB,
                                            SVFBasedPointsToIteratorRef PT) {
  llvm::DenseSet<Policy::o_t> Globals;
  for (const auto &GV : IRDB.getModule()->global_values()) {
    Globals.insert(PT.asAbstractObject(&GV));
  }
  return Globals;
}
} // namespace

TEST(SVFSparsePointsToControlFlowTest, DefiningInstIsAlwaysKept) {
  auto IRDB = LLVMProjectIRDB::loadOrExit(unittest::PathToLLTestFiles +
                                          "pointers/basic_01_c_dbg.ll");
  auto PTOwner = createSVFVFSPointsToInfo(IRDB);
  SVFBasedPointsToIteratorRef PT = PTOwner;
  auto KnownGlobals = globalObjectsOf(IRDB, PT);

  const auto *V = IRDB.getInstruction(5);
  ASSERT_TRUE(V && V->getType()->isPointerTy());

  EXPECT_TRUE(
      Policy::shouldKeepInst(V, PT.asAbstractObject(V), PT, KnownGlobals));
}

TEST(SVFSparsePointsToControlFlowTest, FactAsValueOrObjectAgree) {
  auto IRDB = LLVMProjectIRDB::loadOrExit(unittest::PathToLLTestFiles +
                                          "pointers/basic_01_c_dbg.ll");
  auto PTOwner = createSVFVFSPointsToInfo(IRDB);
  SVFBasedPointsToIteratorRef PT = PTOwner;
  auto KnownGlobals = globalObjectsOf(IRDB, PT);

  const auto *V = IRDB.getInstruction(5);
  ASSERT_TRUE(V && V->getType()->isPointerTy());
  const auto *Entry = &V->getFunction()->getEntryBlock().front();

  // Same fact, once passed as v_t (a raw llvm::Value*), once pre-converted
  // to o_t (an SVF node id) -- must resolve to the same next user.
  const auto *NextFromValue =
      Policy::advanceToNextUser(Entry, V, PT, KnownGlobals);
  const auto *NextFromObject = Policy::advanceToNextUser(
      Entry, PT.asAbstractObject(V), PT, KnownGlobals);
  EXPECT_EQ(NextFromValue, NextFromObject);
}

TEST(SVFSparsePointsToControlFlowTest, GlobalRetainedAtCallSite) {
  // main() never touches g1 at all (not even indirectly through foo()), yet
  // the call must still be conservatively retained for fact g1: the
  // retention rule cannot know that foo() doesn't touch it.
  auto IRDB = LLVMProjectIRDB::loadOrExit(
      unittest::PathToLLTestFiles + "linear_constant/global_11_cpp_dbg.ll");
  auto PTOwner = createSVFVFSPointsToInfo(IRDB);
  SVFBasedPointsToIteratorRef PT = PTOwner;

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
  SparseCFGCache<Policy, SVFBasedPointsToIteratorRef> Cache;
  const auto &SCFG = Cache.getOrCreate(CFG, Main, PT.asAbstractObject(G1), PT);
  EXPECT_EQ(Call, SCFG.nextUserOrNull(Entry));
}

int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
