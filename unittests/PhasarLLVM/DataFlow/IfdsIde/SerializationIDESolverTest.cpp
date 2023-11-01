
#include "phasar/DataFlow/IfdsIde/Solver/IDESolver.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDEExtendedTaintAnalysis.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IFDSTaintAnalysis.h"
#include "phasar/PhasarLLVM/HelperAnalyses.h"
#include "phasar/PhasarLLVM/Passes/ValueAnnotationPass.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/SimpleAnalysisConstructor.h"
#include "phasar/PhasarLLVM/TaintConfig/LLVMTaintConfig.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h"
#include "phasar/Utils/DebugOutput.h"
#include "phasar/Utils/TypeTraits.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FileSystem.h"

#include "TestConfig.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

using namespace psr;
using CallBackPairTy = std::pair<IDEExtendedTaintAnalysis<>::config_callback_t,
                                 IDEExtendedTaintAnalysis<>::config_callback_t>;

/* ============== TEST FIXTURE ============== */
class SerializationFixture : public ::testing::TestWithParam<std::string_view> {
protected:
  static constexpr auto PathToLlFiles =
      PHASAR_BUILD_SUBFOLDER("taint_analysis/");
  const std::vector<std::string> EntryPoints = {"main"};

  void doAnalysis(const llvm::Twine &IRFile, LLVMTaintConfig &Config,
                  bool DumpResults = false) {
    HelperAnalyses HA(PathToLlFiles + IRFile, EntryPoints);

    auto TaintProblem =
        createAnalysisProblem<IFDSTaintAnalysis>(HA, &Config, EntryPoints);

    IDESolver Solver(TaintProblem, &HA.getICFG());
    auto AtomicResults = Solver.solve();

    // create temporary file
    std::error_code ErrCode;
    llvm::SmallString<128> TempPath;
    llvm::sys::fs::createTemporaryFile("phasar_temp_IDESolver", "json",
                                       TempPath);

    if (ErrCode) {
      llvm::report_fatal_error(
          llvm::Twine("Failed to open a temporary file: " + ErrCode.message()));
    }

    // run with interruption
    size_t Counter = 0;
    size_t InterruptionValue = 3;
    // results with interruption(s)
    auto InterruptedResults = [&] {
      {
        IDESolver Solver(TaintProblem, &HA.getICFG());
        auto Result = Solver.solveUntil([&Counter, InterruptionValue]() {
          if (Counter >= InterruptionValue) {
            return true;
          }
          Counter++;
          return false;
        });
        EXPECT_EQ(std::nullopt, Result);
        llvm::outs() << "\n"
                     << "\n"
                     << TempPath << "\n"
                     << "\n";
        llvm::outs().flush();
        Solver.saveDataInJSON(TempPath.c_str());
      }

      {
        IDESolver Solver(TaintProblem, &HA.getICFG());
        llvm::outs() << "Before loadDataFromJSON()"
                     << "\n";
        llvm::outs().flush();
        Solver.loadDataFromJSON(HA.getProjectIRDB(), TempPath.c_str());
        Solver.getEndsummary(HA.getProjectIRDB(), TempPath.c_str());
        return std::move(Solver).continueSolving();
      }
    }();

    for (auto &&Cell : AtomicResults.getAllResultEntries()) {
      auto InteractiveRes =
          InterruptedResults.resultAt(Cell.getRowKey(), Cell.getColumnKey());
      EXPECT_EQ(InteractiveRes, Cell.getValue());
    }
  }
}; // Test Fixture

static constexpr std::string_view ISSTestFiles[] = {
    "taint_1_cpp.ll",  "taint_2_cpp.ll",  "taint_3_cpp.ll", "taint_4_cpp.ll",
    "taint_5_cpp.ll",  "taint_6_cpp.ll",  "taint_7_cpp.ll", "taint_8_cpp.ll",
    "taint_14_cpp.ll", "taint_15_cpp.ll",
};
static LLVMTaintConfig getDefaultConfig() {
  auto SourceCB = [](const llvm::Instruction *Inst) {
    std::set<const llvm::Value *> Ret;
    if (const auto *Call = llvm::dyn_cast<llvm::CallBase>(Inst);
        Call && Call->getCalledFunction() &&
        Call->getCalledFunction()->getName() == "_Z6sourcev") {
      Ret.insert(Call);
    }
    return Ret;
  };
  auto SinkCB = [](const llvm::Instruction *Inst) {
    std::set<const llvm::Value *> Ret;
    if (const auto *Call = llvm::dyn_cast<llvm::CallBase>(Inst);
        Call && Call->getCalledFunction() &&
        Call->getCalledFunction()->getName() == "_Z4sinki") {
      assert(Call->arg_size() > 0);
      Ret.insert(Call->getArgOperand(0));
    }
    return Ret;
  };
  return LLVMTaintConfig(std::move(SourceCB), std::move(SinkCB));
}
TEST_P(SerializationFixture, CompareFullRunVSReloadedRun) {
  auto TC = getDefaultConfig();
  doAnalysis(GetParam(), TC);
}

INSTANTIATE_TEST_SUITE_P(InteractiveIDESolverTest, SerializationFixture,
                         ::testing::ValuesIn(ISSTestFiles));

// main function for the test case
int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
