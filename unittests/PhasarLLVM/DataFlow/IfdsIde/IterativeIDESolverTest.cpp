
#include "phasar/DataFlow/IfdsIde/Solver/IterativeIDESolver.h"

#include "phasar/DataFlow/IfdsIde/Solver/DemandIdBasedSolverResults.h"
#include "phasar/DataFlow/IfdsIde/Solver/GenericSolverResults.h"
#include "phasar/DataFlow/IfdsIde/Solver/IDESolver.h"
#include "phasar/DataFlow/IfdsIde/Solver/StaticIDESolverConfig.h"
#include "phasar/DataFlow/IfdsIde/SolverResults.h"
#include "phasar/Domain/LatticeDomain.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IFDSTaintAnalysis.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/TaintConfig/LLVMTaintConfig.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include "TaintUtils.h"
#include "TestConfig.h"
#include "gtest/gtest.h"

#include <chrono>
#include <type_traits>

using namespace psr;

namespace {

/* ============== TEST FIXTURE ============== */
class IterativeIDESolverLCATest
    : public ::testing::TestWithParam<std::string_view> {
protected:
  template <typename SolverConfigTy = IDESolverConfig>
  void doAnalysis(const llvm::Twine &LlvmFilePath, bool PrintDump = false) {
    LLVMProjectIRDB IRDB(unittest::PathToLLTestFiles + LlvmFilePath);
    DIBasedTypeHierarchy TH(IRDB);
    LLVMAliasSet PT(&IRDB);
    LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::OTF, {"main"}, &TH, &PT,
                       Soundness::Soundy, /*IncludeGlobals*/ true);

    IDELinearConstantAnalysis Problem(&IRDB, &ICFG, {"main"});
    IterativeIDESolver<IDELinearConstantAnalysis, SolverConfigTy> Solver(
        &Problem, &ICFG);

    auto Start = std::chrono::steady_clock::now();
    Solver.solve();
    auto End = std::chrono::steady_clock::now();
    auto NewTime = End - Start;
    llvm::errs() << "IterativeIDESolver Elapsed:\t" << NewTime.count()
                 << "ns\n";

    IDESolver OldSolver(&Problem, &ICFG);
    Start = std::chrono::steady_clock::now();
    OldSolver.solve();
    End = std::chrono::steady_clock::now();

    auto OldTime = End - Start;
    llvm::errs() << "IDESolver Elapsed:\t\t" << OldTime.count() << "ns\n";

    if (PrintDump) {
      Solver.dumpResults();
      OldSolver.dumpResults();
    }

    checkEquality(OldSolver.getSolverResults(), Solver.getSolverResults(),
                  SolverConfigTy{});

    [[maybe_unused]] GenericSolverResults<const llvm::Instruction *,
                                          const llvm::Value *,
                                          LatticeDomain<int64_t>> SR =
        OldSolver.getSolverResults();
    [[maybe_unused]] GenericSolverResults<
        const llvm::Instruction *, const llvm::Value *,
        std::conditional_t<SolverConfigTy::ComputeValues,
                           LatticeDomain<int64_t>, BinaryDomain>> SR2 =
        Solver.getSolverResults();
  }

  struct IDEDemandConfig : IDESolverConfig {
    static constexpr bool ComputeResultsTable = false;
  };

  void doAnalysisDemandPhase2(const llvm::Twine &LlvmFilePath,
                              bool PrintDump = false) {
    LLVMProjectIRDB IRDB(unittest::PathToLLTestFiles + LlvmFilePath);
    DIBasedTypeHierarchy TH(IRDB);
    LLVMAliasSet PT(&IRDB);
    LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::OTF, {"main"}, &TH, &PT,
                       Soundness::Soundy, /*IncludeGlobals*/ true);

    IDELinearConstantAnalysis Problem(&IRDB, &ICFG, {"main"});
    IterativeIDESolver<IDELinearConstantAnalysis, IDEDemandConfig> Solver(
        &Problem, &ICFG);

    auto Start = std::chrono::steady_clock::now();
    Solver.solve();
    auto End = std::chrono::steady_clock::now();
    auto NewTime = End - Start;
    llvm::errs() << "IterativeIDESolver Elapsed:\t" << NewTime.count()
                 << "ns\n";

    IDESolver OldSolver(&Problem, &ICFG);
    Start = std::chrono::steady_clock::now();
    OldSolver.solve();
    End = std::chrono::steady_clock::now();

    auto OldTime = End - Start;
    llvm::errs() << "IDESolver Elapsed:\t\t" << OldTime.count() << "ns\n";

    if (PrintDump) {
      Solver.dumpResults();
      OldSolver.dumpResults();
    }

    EXPECT_TRUE(std::empty(Solver.getSolverResults().getAllResultEntries()));

    checkDemandEquality(OldSolver.getSolverResults(),
                        DemandIdBasedSolverResults(&Solver));
  }

  struct NonGCIFDSSolverConfig : IFDSSolverConfig {
    static inline constexpr auto EnableJumpFunctionGC =
        JumpFunctionGCMode::Disabled;
  };

  template <typename SR1, typename SR2>
  void checkEquality(const SR1 &LHS, const SR2 &RHS, IDESolverConfig /*Tag*/) {
    llvm::errs() << "IDE Equality Check\n";
    EXPECT_EQ(LHS.size(), RHS.size())
        << "The instructions, where results are computed differ";

    for (const auto &[Row, ColVal] : LHS.rowMapView()) {
      EXPECT_TRUE(RHS.containsNode(Row))
          << "The RHS does not contain results at inst " << llvmIRToString(Row);

      auto RHSColVal = RHS.row(Row);
      EXPECT_EQ(ColVal.size(), RHSColVal.size())
          << "The number of dataflow facts at inst " << llvmIRToString(Row)
          << " do not match";

      for (const auto &[Col, Val] : ColVal) {
        auto It = RHSColVal.find(Col);

        EXPECT_TRUE(It != RHSColVal.end())
            << "The RHS does not contain fact " << llvmIRToString(Col)
            << " at inst " << llvmIRToString(Row);
        if (It != RHSColVal.end()) {
          EXPECT_TRUE(Val == It->second)
              << "The edge values at inst " << llvmIRToString(Row)
              << " and fact " << llvmIRToString(Col) << " do not match: " << Val
              << " vs " << It->second;
        }
      }
    }
  }

  template <typename SR1, typename SR2>
  void checkEquality(const SR1 &LHS, const SR2 &RHS, IFDSSolverConfig /*Tag*/) {
    llvm::errs() << "IFDS Equality Check\n";
    EXPECT_EQ(LHS.size(), RHS.size())
        << "The instructions, where results are computed differ";

    for (const auto &[Row, ColVal] : LHS.rowMapView()) {
      EXPECT_TRUE(RHS.containsNode(Row))
          << "The RHS does not contain results at inst " << llvmIRToString(Row);

      auto RHSColVal = RHS.row(Row);
      EXPECT_EQ(ColVal.size(), RHSColVal.size())
          << "The number of dataflow facts at inst " << llvmIRToString(Row)
          << " do not match";

      for (const auto &[Col, Val] : ColVal) {
        EXPECT_TRUE(RHSColVal.count(Col))
            << "The RHS does not contain fact " << llvmIRToString(Col)
            << " at inst " << llvmIRToString(Row);
      }
    }
  }

  template <typename SR1, typename DSR>
  void checkDemandEquality(const SR1 &LHS, DSR &&RHS) {
    llvm::errs() << "Demand IDE Equality Check\n";

    for (const auto &[Row, ColVal] : LHS.rowMapView()) {
      EXPECT_TRUE(RHS.containsNode(Row))
          << "The RHS does not contain results at inst " << llvmIRToString(Row);

      auto RHSColVal = RHS.ifdsResultsAt(Row);
      EXPECT_EQ(ColVal.size(), RHSColVal.size())
          << "The number of dataflow facts at inst " << llvmIRToString(Row)
          << " do not match";

      for (const auto &[Col, Val] : ColVal) {
        auto DemandRes = RHS.resultAt(Row, Col);

        EXPECT_TRUE(Val == DemandRes)
            << "The edge values at inst " << llvmIRToString(Row) << " and fact "
            << llvmIRToString(Col) << " do not match: " << Val << " vs "
            << DemandRes;
      }
    }
  }

  void TearDown() override {}

}; // Test Fixture

class IterativeIDESolverTATest
    : public ::testing::TestWithParam<std::string_view> {
protected:
  struct IFDSTAConfig : IFDSSolverConfig {
    static constexpr bool ComputeResultsTable = false;
  };

  void doIFDSTaintAnalysis(const llvm::Twine &LlvmFilePath,
                           bool PrintDump = false) {
    auto IRDB =
        LLVMProjectIRDB::loadOrExit(unittest::PathToLLTestFiles + LlvmFilePath);
    DIBasedTypeHierarchy TH(IRDB);
    LLVMAliasSet PT(&IRDB);
    LLVMBasedICFG ICFG(&IRDB, CallGraphAnalysisType::OTF, {"main"}, &TH, &PT,
                       Soundness::Soundy, /*IncludeGlobals*/ true);

    auto TC = unittest::getDefaultConfig();
    IFDSTaintAnalysis NewTA(&IRDB, &PT, &TC, {"main"});
    IFDSTaintAnalysis OldTA(&IRDB, &PT, &TC, {"main"});

    IterativeIDESolver<IFDSTaintAnalysis, IFDSTAConfig> Solver(&NewTA, &ICFG);

    auto Start = std::chrono::steady_clock::now();
    Solver.solve();
    auto End = std::chrono::steady_clock::now();
    auto NewTime = End - Start;
    llvm::errs() << "IterativeIDESolver Elapsed:\t" << NewTime.count()
                 << "ns\n";

    IDESolver OldSolver(&OldTA, &ICFG);
    Start = std::chrono::steady_clock::now();
    OldSolver.solve();
    End = std::chrono::steady_clock::now();

    auto OldTime = End - Start;
    llvm::errs() << "IDESolver Elapsed:\t\t" << OldTime.count() << "ns\n";

    if (PrintDump) {
      Solver.dumpResults();
      OldSolver.dumpResults();
    }

    EXPECT_EQ(OldTA.Leaks, NewTA.Leaks);
  }
};

/// --> Using IDESolverConfig

constexpr std::string_view LCASources[] = {
    "control_flow/branch_cpp.ll",
    "linear_constant/call_06_cpp.ll",
    "linear_constant/call_07_cpp.ll",
    "linear_constant/call_08_cpp.ll",
    "linear_constant/call_09_cpp.ll",
    "linear_constant/call_10_cpp.ll",
    "linear_constant/call_11_cpp.ll",
    "linear_constant/call_12_cpp.ll",
    "linear_constant/recursion_01_cpp.ll",
    "linear_constant/recursion_02_cpp.ll",
    "linear_constant/recursion_03_cpp.ll",
    "linear_constant/global_01_cpp.ll",
    "linear_constant/global_02_cpp.ll",
    "linear_constant/global_03_cpp.ll",
    "linear_constant/global_04_cpp.ll",
    "linear_constant/global_05_cpp.ll",
    "linear_constant/global_06_cpp.ll",
    "linear_constant/global_07_cpp.ll",
    "linear_constant/global_08_cpp.ll",
    "linear_constant/global_09_cpp.ll",
    "linear_constant/global_10_cpp.ll",
    "linear_constant/external_fun_cpp.ll",
};

TEST_P(IterativeIDESolverLCATest, IDESolverTest) { doAnalysis(GetParam()); }

TEST_P(IterativeIDESolverLCATest, IFDSSolverTest) {
  doAnalysis<IFDSSolverConfig>(GetParam());
}

TEST_P(IterativeIDESolverLCATest, DemandIDESolverTest) {
  doAnalysisDemandPhase2(GetParam());
}

constexpr std::string_view DummyTASources[] = {
    "taint_01_cpp_dbg.ll",           "taint_01_cpp_m2r_dbg.ll",
    "taint_02_cpp_dbg.ll",           "taint_03_cpp_dbg.ll",
    "taint_04_cpp_dbg.ll",           "taint_05_cpp_dbg.ll",
    "taint_06_cpp_m2r_dbg.ll",       "sret_c_dbg.ll",
    "taint_exception_01_cpp_dbg.ll", "taint_exception_01_cpp_m2r_dbg.ll",
    "taint_exception_02_cpp_dbg.ll", "taint_exception_03_cpp_dbg.ll",
    "taint_exception_04_cpp_dbg.ll", "taint_exception_05_cpp_dbg.ll",
    "taint_exception_06_cpp_dbg.ll", "taint_exception_07_cpp_dbg.ll",
    "taint_exception_08_cpp_dbg.ll", "taint_exception_09_cpp_dbg.ll",
    "taint_exception_10_cpp_dbg.ll", "taint_lib_sum_01_cpp_dbg.ll",
};

TEST_P(IterativeIDESolverTATest, TaintTest_01) {
  doIFDSTaintAnalysis("taint_analysis/dummy_source_sink/" +
                      llvm::Twine(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(IterativeIDESolverTest, IterativeIDESolverLCATest,
                         ::testing::ValuesIn(LCASources));
INSTANTIATE_TEST_SUITE_P(IterativeIDESolverTest, IterativeIDESolverTATest,
                         ::testing::ValuesIn(DummyTASources));

} // namespace

int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
