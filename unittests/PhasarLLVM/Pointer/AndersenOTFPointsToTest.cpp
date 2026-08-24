#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Pointer/AndersenOTFAA.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointerAssignmentGraph.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToInfo.h"
#include "phasar/Pointer/RawPointsToResult.h"
#include "phasar/Utils/IotaIterator.h"
#include "phasar/Utils/ValueCompressor.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"

#include "SrcCodeLocationEntry.h"
#include "TestConfig.h"
#include "gtest/gtest.h"

#include <algorithm>

namespace {
using namespace psr;
using namespace psr::unittest;

static_assert(RawPointsToResult<AndersenOTFPointsToResult>);

constexpr auto PathToLLFiles = PHASAR_BUILD_SUBFOLDER("pointers/");

using TSL = TestingSrcLocation;
using ValueSet = llvm::SmallDenseSet<const llvm::Value *, 4>;

template <typename PTIterT>
[[nodiscard]] ValueSet pointeesOf(const PTIterT &PTI, const llvm::Value *Ptr) {
  ValueSet Ret;
  PTI.forallPointeesOf(Ptr, nullptr, [&Ret](const llvm::Value *Pointee) {
    Ret.insert(Pointee);
  });
  return Ret;
}

TEST(AndersenOTFPointsToTest, AllocaAndCallResult) {
  // retptr(&a) returns &a, so the formal parameter and the call result both
  // point to a's storage -- which is its own abstract object.
  auto IRDB = LLVMProjectIRDB::loadOrExit(
      PathToLLFiles + llvm::Twine("andersen_otf_interproc_c_m2r_dbg.ll"));

  const auto *MainFn = IRDB.getFunctionDefinition("main");
  ASSERT_NE(MainFn, nullptr);

  // 'int *q = retptr(p);'
  constexpr LineColFunOp CallLoc{
      .Line = 9, .InFunction = "main", .OpCode = llvm::Instruction::Call};

  const auto *Call = testingLocInIR(CallLoc, IRDB);
  const auto *AddrOfA =
      testingLocInIR(OperandOf{.OperandIndex = 0, .Inst = CallLoc}, IRDB);
  const auto *FormalX =
      testingLocInIR(ArgInFun{.Idx = 0, .InFunction = "retptr"}, IRDB);

  auto PTI = computeAndersenOTFPointsTo(IRDB, {MainFn});

  EXPECT_EQ(pointeesOf(PTI, AddrOfA), ValueSet({AddrOfA}))
      << "an allocation site is its own -- and only -- abstract object";
  EXPECT_TRUE(pointeesOf(PTI, Call).contains(AddrOfA));
  EXPECT_TRUE(pointeesOf(PTI, FormalX).contains(AddrOfA));
  EXPECT_TRUE(PTI.mayPointsTo(Call, AddrOfA));
  EXPECT_FALSE(PTI.mayPointsTo(AddrOfA, Call));
}

TEST(AndersenOTFPointsToTest, GlobalContentsViaLoad) {
  // 'int *p = &x': only the value loaded from @p points to @x -- pts(@p) is
  // @p's own object, as the contents of an object are not queryable.
  auto IRDB = LLVMProjectIRDB::loadOrExit(
      PathToLLFiles + llvm::Twine("andersen_otf_global_init_c_dbg.ll"));

  const auto *MainFn = IRDB.getFunctionDefinition("main");
  ASSERT_NE(MainFn, nullptr);

  // 'int *q = p;'
  const auto *Load = testingLocInIR(
      LineColFunOp{
          .Line = 7, .InFunction = "main", .OpCode = llvm::Instruction::Load},
      IRDB);
  const auto *GlobalX = testingLocInIR(GlobalVar{.Name = "x"}, IRDB);
  const auto *GlobalP = testingLocInIR(GlobalVar{.Name = "p"}, IRDB);

  auto PTI = computeAndersenOTFPointsTo(IRDB, {MainFn});

  EXPECT_TRUE(pointeesOf(PTI, Load).contains(GlobalX));
  EXPECT_TRUE(pointeesOf(PTI, GlobalP).contains(GlobalP));
}

TEST(AndersenOTFPointsToTest, FunctionPointer) {
  // The OTF fixpoint resolves fp to id, so pts(fp) contains the function.
  // Needs the non-mem2reg IR: promoting fp turns the call into a direct one.
  auto IRDB = LLVMProjectIRDB::loadOrExit(
      PathToLLFiles + llvm::Twine("andersen_otf_fp_c_dbg.ll"));

  const auto *MainFn = IRDB.getFunctionDefinition("main");
  ASSERT_NE(MainFn, nullptr);

  // 'int *q = fp(p);' -- operand 1 is the callee of this one-argument call.
  constexpr LineColFunOp CallLoc{
      .Line = 11, .InFunction = "main", .OpCode = llvm::Instruction::Call};

  const auto *Callee =
      testingLocInIR(OperandOf{.OperandIndex = 1, .Inst = CallLoc}, IRDB);
  const auto *IdFn = testingLocInIR(FuncByName{.FuncName = "id"}, IRDB);

  auto PTI = computeAndersenOTFPointsTo(IRDB, {MainFn});

  EXPECT_TRUE(pointeesOf(PTI, Callee).contains(IdFn));
  EXPECT_EQ(pointeesOf(PTI, IdFn), ValueSet({IdFn}))
      << "a function is its own abstract object";
  EXPECT_FALSE(pointeesOf(PTI, IdFn).contains(Callee))
      << "the load folded into id's node is not an allocation site";
}

TEST(AndersenOTFPointsToTest, PointsToImpliesAlias) {
  // Two pointers alias iff their points-to sets intersect, so the points-to
  // export must agree with the alias export it is derived from.
  for (llvm::StringRef File :
       {"andersen_otf_interproc_c_m2r_dbg.ll",
        "andersen_otf_merge_load_c_dbg.ll", "andersen_otf_fp_c_dbg.ll"}) {
    auto IRDB = LLVMProjectIRDB::loadOrExit(PathToLLFiles + llvm::Twine(File));
    const auto *MainFn = IRDB.getFunctionDefinition("main");
    ASSERT_NE(MainFn, nullptr);

    ValueCompressor<PAGVariable> Compressor;
    auto AliasRes = computeAndersenOTFRaw(IRDB, {MainFn}, &Compressor);
    auto PtsRes = computeAndersenOTFPointsToRaw(IRDB, {MainFn}, &Compressor);

    EXPECT_EQ(AliasRes.size(), PtsRes.size())
        << "naming abstract objects must not create external ids: "
        << File.str();

    const auto NumIds = std::min(AliasRes.size(), PtsRes.size());
    for (auto Ptr1 : iota<ValueId>(NumIds)) {
      const auto Pts1 = PtsRes.getRawPointsToSet(Ptr1);
      if (Pts1.empty()) {
        continue;
      }
      for (auto Ptr2 : iota<ValueId>(NumIds)) {
        auto Common = PtsRes.getRawPointsToSet(Ptr2);
        Common &= Pts1;
        if (Common.empty()) {
          continue;
        }
        EXPECT_TRUE(AliasRes.mayAlias(Ptr1, Ptr2))
            << File.str() << ": #" << uint32_t(Ptr1) << " and #"
            << uint32_t(Ptr2) << " share an abstract object, but do not alias";
      }
    }
  }
}

TEST(AndersenOTFPointsToTest, IteratorRefCompatibility) {
  auto IRDB = LLVMProjectIRDB::loadOrExit(
      PathToLLFiles + llvm::Twine("andersen_otf_interproc_c_m2r_dbg.ll"));

  const auto *MainFn = IRDB.getFunctionDefinition("main");
  ASSERT_NE(MainFn, nullptr);

  constexpr LineColFunOp CallLoc{
      .Line = 9, .InFunction = "main", .OpCode = llvm::Instruction::Call};
  const auto *Call = testingLocInIR(CallLoc, IRDB);
  const auto *AddrOfA =
      testingLocInIR(OperandOf{.OperandIndex = 0, .Inst = CallLoc}, IRDB);

  auto Owned = createAndersenOTFPointsToIterator(IRDB, {MainFn});
  LLVMPointsToIteratorRef PTI = Owned;

  ValueSet Pointees;
  PTI.forallPointeesOf(Call, nullptr, [&Pointees](const llvm::Value *Pointee) {
    Pointees.insert(Pointee);
  });

  EXPECT_TRUE(Pointees.contains(AddrOfA));
  EXPECT_TRUE(PTI.mayPointsTo(Call, AddrOfA, nullptr));
  EXPECT_EQ(PTI.asAbstractObject(AddrOfA), AddrOfA);
}

} // namespace

int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
