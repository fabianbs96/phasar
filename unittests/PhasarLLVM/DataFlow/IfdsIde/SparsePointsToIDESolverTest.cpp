#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/ControlFlow/SparseCFGProvider.h"
#include "phasar/DataFlow/IfdsIde/Solver/IDESolver.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparsePointsToBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include "TestConfig.h"
#include "gtest/gtest.h"

using namespace psr;
namespace {

class LinearConstant : public ::testing::TestWithParam<std::string_view> {
protected:
  const std::vector<std::string> EntryPoints = {"main"};
};

TEST_P(LinearConstant, SparseResultsEquivalent) {
  static constexpr auto PathToLlFiles =
      PHASAR_BUILD_SUBFOLDER("linear_constant/");
  LLVMProjectIRDB IRDB(PathToLlFiles + GetParam());
  DIBasedTypeHierarchy TH(IRDB);
  LLVMAliasSet PT(&IRDB);

  LLVMBasedICFG ICF(&IRDB, CallGraphAnalysisType::OTF, EntryPoints, &TH, &PT);
  auto HasGlobalCtor = IRDB.getFunctionDefinition(
                           LLVMBasedICFG::GlobalCRuntimeModelName) != nullptr;
  std::vector Entry = {
      HasGlobalCtor ? LLVMBasedICFG::GlobalCRuntimeModelName.str() : "main"};

  // LLVMAliasSet converts to LLVMPointsToIteratorRef via its
  // getReachableAllocationSites() API.
  LLVMSparsePointsToBasedICFG SICF(ICF.getCallGraph(), &IRDB, &PT);

  static_assert(
      has_getSparseCFG_v<LLVMSparsePointsToBasedICFG, const llvm::Value *>);

  IDELinearConstantAnalysis LCAProblem(&IRDB, &ICF, Entry);
  IDELinearConstantAnalysis SLCAProblem(&IRDB, &SICF, Entry);

  auto DenseResults = IDESolver(&LCAProblem, &ICF).solve();
  auto SparseResults = IDESolver(&SLCAProblem, &SICF).solve();

  for (auto &&Cell : SparseResults.getAllResultEntries()) {
    auto DenseRes =
        DenseResults.resultAt(Cell.getRowKey(), Cell.getColumnKey());
    EXPECT_EQ(DenseRes, Cell.getValue())
        << "At " << llvmIRToString(Cell.getRowKey())
        << " :: " << llvmIRToShortString(Cell.getColumnKey());
  }
  // Note: Do not check for equivalence, because SparseIDE is *expected* to
  // compute less (N, D) results than vanilla IDE.
}

static constexpr std::string_view LCATestFiles[] = {
    "basic_01_cpp_dbg.ll",  "basic_02_cpp_dbg.ll", "basic_03_cpp_dbg.ll",
    "branch_01_cpp_dbg.ll", "while_01_cpp_dbg.ll", "call_01_cpp_dbg.ll",
    "global_01_cpp_dbg.ll",
};

INSTANTIATE_TEST_SUITE_P(SparsePointsToIDETest, LinearConstant,
                         ::testing::ValuesIn(LCATestFiles));
} // namespace

int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
