/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/DataFlow/IfdsIde/Solver/DFISolver.h"

#include "phasar/DataFlow/IfdsIde/Solver/GenericSolverResults.h"
#include "phasar/DataFlow/IfdsIde/Solver/IFDSSolver.h"
#include "phasar/DataFlow/IfdsIde/Solver/IterativeIDESolver.h"
#include "phasar/DataFlow/IfdsIde/Solver/StaticIDESolverConfig.h"
#include "phasar/Domain/BinaryDomain.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IFDSTaintAnalysis.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IFDSUninitializedVariables.h"
#include "phasar/PhasarLLVM/HelperAnalyses.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/SimpleAnalysisConstructor.h"
#include "phasar/PhasarLLVM/TaintConfig/LLVMTaintConfig.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/DebugOutput.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"

#include "TaintTest.h"
#include "TestConfig.h"
#include "gtest/gtest.h"

#include <string>
#include <string_view>
#include <vector>

namespace {

using n_t = const llvm::Instruction *;
using d_t = const llvm::Value *;

void compareResults(const psr::LLVMProjectIRDB &IRDB, const auto &Expected,
                    const auto &Actual, d_t IgnoredFact = nullptr) {
  for (const auto *Inst : IRDB.getAllInstructions()) {
    auto ExpectedFacts = Expected.ifdsResultsAt(Inst);
    auto ActualFacts = Actual.ifdsResultsAt(Inst);
    ExpectedFacts.erase(IgnoredFact);
    ActualFacts.erase(IgnoredFact);
    EXPECT_EQ(ExpectedFacts, ActualFacts)
        << "Results differ at " << psr::llvmIRToString(Inst)
        << "\n  Expected: " << psr::PrettyPrinter{ExpectedFacts}
        << "\n  Got: " << psr::PrettyPrinter{ActualFacts};
  }
}

/// All materialized facts must be reachable from a seed
template <typename ProblemTy>
void checkReachableFromSeeds(const psr::LLVMProjectIRDB &IRDB,
                             ProblemTy &Problem,
                             const psr::DFISolverResults<n_t, d_t> &Results,
                             const psr::DFIReachability<n_t, d_t> &Reach) {
  llvm::SmallVector<psr::dfi::VertexId> SeedVertices;
  for (const auto &[Inst, Facts] : Problem.initialSeeds().getSeeds()) {
    for (const auto &[Fact, Value] : Facts) {
      auto Vtx = Reach.find(Inst, Fact);
      ASSERT_TRUE(Vtx.has_value()) << "Seed not materialized";
      SeedVertices.push_back(*Vtx);
    }
  }

  for (const auto *Inst : IRDB.getAllInstructions()) {
    for (auto Vtx : Results.verticesAt(Inst)) {
      EXPECT_TRUE(llvm::any_of(
          SeedVertices,
          [&](psr::dfi::VertexId Seed) { return Reach.reaches(Seed, Vtx); }))
          << "Fact " << psr::llvmIRToString(Reach.factOf(Vtx)) << " at "
          << psr::llvmIRToString(Inst) << " not reachable from any seed";
      EXPECT_TRUE(Reach.reaches(Vtx, Vtx));
    }
  }
}

class DFISolverLCATest : public ::testing::TestWithParam<std::string_view> {
protected:
  static constexpr auto PathToLlFiles =
      PHASAR_BUILD_SUBFOLDER("linear_constant/");
};

TEST_P(DFISolverLCATest, SameResultsAsIterativeIFDSSolver) {
  psr::LLVMProjectIRDB IRDB(PathToLlFiles + GetParam());
  psr::DIBasedTypeHierarchy TH(IRDB);
  psr::LLVMAliasSet PT(&IRDB);
  psr::LLVMBasedICFG ICFG(&IRDB, psr::CallGraphAnalysisType::OTF, {"main"}, &TH,
                          &PT, psr::Soundness::Soundy,
                          /*IncludeGlobals*/ true);
  psr::IDELinearConstantAnalysis Problem(&IRDB, &ICFG, {"main"});

  psr::IterativeIDESolver IterSolver(&Problem, &ICFG, psr::IFDSSolverConfig{});
  IterSolver.solve();

  psr::DFISolver Solver(&Problem, &ICFG);
  auto Results = Solver.solve();

  compareResults(IRDB, IterSolver.getSolverResults(), Results);
  checkReachableFromSeeds(IRDB, Problem, Results, Solver.getReachability());

  [[maybe_unused]] psr::GenericSolverResults<n_t, d_t, psr::BinaryDomain>
      Generic = Results;
}

TEST_P(DFISolverLCATest, OwningResultsWithoutIndex) {
  psr::LLVMProjectIRDB IRDB(PathToLlFiles + GetParam());
  psr::DIBasedTypeHierarchy TH(IRDB);
  psr::LLVMAliasSet PT(&IRDB);
  psr::LLVMBasedICFG ICFG(&IRDB, psr::CallGraphAnalysisType::OTF, {"main"}, &TH,
                          &PT, psr::Soundness::Soundy,
                          /*IncludeGlobals*/ true);
  psr::IDELinearConstantAnalysis Problem(&IRDB, &ICFG, {"main"});

  psr::IterativeIDESolver IterSolver(&Problem, &ICFG, psr::IFDSSolverConfig{});
  IterSolver.solve();

  auto Results =
      psr::solveDFIProblem(Problem, ICFG, psr::DFISolverConfigNoIndex{});

  compareResults(IRDB, IterSolver.getSolverResults(), Results);
}

constexpr std::string_view LCATestFiles[] = {
    "basic_01_cpp_dbg.ll",
    "basic_02_cpp_dbg.ll",
    "basic_03_cpp_dbg.ll",
    "basic_04_cpp_dbg.ll",
    "basic_05_cpp_dbg.ll",
    "basic_06_cpp_dbg.ll",
    "basic_07_cpp_dbg.ll",
    "basic_08_cpp_dbg.ll",
    "basic_09_cpp_dbg.ll",
    "basic_10_cpp_dbg.ll",
    "basic_11_cpp_dbg.ll",
    "basic_12_cpp_dbg.ll",

    "branch_01_cpp_dbg.ll",
    "branch_02_cpp_dbg.ll",
    "branch_03_cpp_dbg.ll",
    "branch_04_cpp_dbg.ll",
    "branch_05_cpp_dbg.ll",
    "branch_06_cpp_dbg.ll",
    "branch_07_cpp_dbg.ll",

    "while_01_cpp_dbg.ll",
    "while_02_cpp_dbg.ll",
    "while_03_cpp_dbg.ll",
    "while_04_cpp_dbg.ll",
    "while_05_cpp_dbg.ll",
    "for_01_cpp_dbg.ll",

    "call_01_cpp_dbg.ll",
    "call_02_cpp_dbg.ll",
    "call_03_cpp_dbg.ll",
    "call_04_cpp_dbg.ll",
    "call_05_cpp_dbg.ll",
    "call_06_cpp_dbg.ll",
    "call_07_cpp_dbg.ll",
    "call_08_cpp_dbg.ll",
    "call_09_cpp_dbg.ll",
    "call_10_cpp_dbg.ll",
    "call_11_cpp_dbg.ll",

    "recursion_01_cpp_dbg.ll",
    "recursion_02_cpp_dbg.ll",
    "recursion_03_cpp_dbg.ll",

    "global_01_cpp_dbg.ll",
    "global_02_cpp_dbg.ll",
    "global_03_cpp_dbg.ll",
    "global_04_cpp_dbg.ll",
    "global_05_cpp_dbg.ll",
    "global_06_cpp_dbg.ll",
    "global_07_cpp_dbg.ll",
    "global_08_cpp_dbg.ll",
    "global_09_cpp_dbg.ll",
    "global_10_cpp_dbg.ll",
    "global_11_cpp_dbg.ll",
    "global_12_cpp_dbg.ll",
    "global_13_cpp_dbg.ll",
    "global_14_cpp_dbg.ll",
    "global_15_cpp_dbg.ll",
    "global_16_cpp_dbg.ll",

    "overflow_add_cpp_dbg.ll",
    "overflow_sub_cpp_dbg.ll",
    "overflow_mul_cpp_dbg.ll",
    "overflow_div_min_by_neg_one_cpp_dbg.ll",

    "ub_division_by_zero_cpp_dbg.ll",
    "ub_modulo_by_zero_cpp_dbg.ll",
    "external_fun_cpp.ll",
};

INSTANTIATE_TEST_SUITE_P(DFISolverLCATest, DFISolverLCATest,
                         ::testing::ValuesIn(LCATestFiles));

class DFISolverTaintTest : public ::testing::TestWithParam<std::string_view> {
protected:
  static constexpr auto PathToLlFiles =
      PHASAR_BUILD_SUBFOLDER("taint_analysis/dummy_source_sink/");
  static inline const std::vector<std::string> EntryPoints = {"main"};
};

/// Leaks are reported as side effects of the flow functions
TEST_P(DFISolverTaintTest, SameLeaksAsIterativeIDESolver) {
  psr::HelperAnalyses HA(PathToLlFiles + GetParam(), EntryPoints);
  auto Config = psr::unittest::getDefaultConfig();

  auto ExpectedProblem = psr::createAnalysisProblem<psr::IFDSTaintAnalysis>(
      HA, &Config, EntryPoints);
  psr::IterativeIDESolver ExpectedSolver(&ExpectedProblem, &HA.getICFG(),
                                         psr::IFDSSolverConfig{});
  auto ExpectedResults = ExpectedSolver.solve();

  auto Problem = psr::createAnalysisProblem<psr::IFDSTaintAnalysis>(
      HA, &Config, EntryPoints);
  psr::DFISolver Solver(&Problem, &HA.getICFG());
  auto Results = Solver.solve();

  EXPECT_EQ(ExpectedProblem.Leaks, Problem.Leaks)
      << "Leaks differ:\n  Expected: "
      << psr::PrettyPrinter{ExpectedProblem.Leaks}
      << "\n  Got: " << psr::PrettyPrinter{Problem.Leaks};

  compareResults(HA.getProjectIRDB(), ExpectedResults, Results);
  checkReachableFromSeeds(HA.getProjectIRDB(), Problem, Results,
                          Solver.getReachability());
}

constexpr std::string_view TaintTestFiles[] = {
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

INSTANTIATE_TEST_SUITE_P(DFISolverTaintTest, DFISolverTaintTest,
                         ::testing::ValuesIn(TaintTestFiles));

struct UnbalancedTestCase {
  std::string_view File;
  std::string_view EntryPoint;
};

class DFISolverUnbalancedTest
    : public ::testing::TestWithParam<UnbalancedTestCase> {
protected:
  static constexpr auto PathToLlFiles =
      PHASAR_BUILD_SUBFOLDER("uninitialized_variables/");
  static inline const std::vector<std::string> ICFGEntryPoints = {"main"};
};

/// The IterativeIDESolver does not support followReturnsPastSeeds, so compare
/// against the IFDSSolver
TEST_P(DFISolverUnbalancedTest, SameResultsAsIFDSSolver) {
  const auto &[File, EntryPoint] = GetParam();
  // The ICFG starts at main, such that the seeded function has callers
  psr::HelperAnalyses HA(PathToLlFiles + File, ICFGEntryPoints);
  const auto &IRDB = HA.getProjectIRDB();
  std::vector<std::string> ProblemEntryPoints = {std::string(EntryPoint)};

  auto ExpectedProblem =
      psr::createAnalysisProblem<psr::IFDSUninitializedVariables>(
          HA, ProblemEntryPoints);
  ExpectedProblem.getIFDSIDESolverConfig().setFollowReturnsPastSeeds(true);
  psr::IFDSSolver ExpectedSolver(&ExpectedProblem, &HA.getICFG());
  auto ExpectedResults = ExpectedSolver.solve();

  auto Problem = psr::createAnalysisProblem<psr::IFDSUninitializedVariables>(
      HA, ProblemEntryPoints);
  Problem.getIFDSIDESolverConfig().setFollowReturnsPastSeeds(true);
  psr::DFISolver Solver(&Problem, &HA.getICFG());
  auto Results = Solver.solve();

  // The IDESolver drops the zero fact at call sites and exits that are only
  // reachable via unbalanced returns, as its value computation (phase II)
  // starts at the start points only
  compareResults(IRDB, ExpectedResults, Results, Problem.getZeroValue());
  checkReachableFromSeeds(IRDB, Problem, Results, Solver.getReachability());
  EXPECT_EQ(ExpectedProblem.getAllUndefUses(), Problem.getAllUndefUses());

  // Unbalanced returns into main, if any, also happen in the DFISolver
  const auto *Main = IRDB.getFunctionDefinition("main");
  ASSERT_NE(Main, nullptr);
  const auto HasResultsInMain = [&](const auto &SR) {
    return llvm::any_of(IRDB.getAllInstructionsOf(Main), [&](const auto *Inst) {
      return !SR.ifdsResultsAt(Inst).empty();
    });
  };
  EXPECT_EQ(HasResultsInMain(ExpectedResults), HasResultsInMain(Results));
}

constexpr UnbalancedTestCase UnbalancedTestCases[] = {
    {"multiple_calls_cpp_dbg.ll", "_Z8functionv"},
    {"recursion_cpp_dbg.ll", "_Z3fooRii"},
    {"callsite_cpp_dbg.ll", "_Z3foov"},
    {"return_uninit_cpp_dbg.ll", "_Z3foov"},
};

INSTANTIATE_TEST_SUITE_P(DFISolverUnbalancedTest, DFISolverUnbalancedTest,
                         ::testing::ValuesIn(UnbalancedTestCases));

} // namespace

int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
