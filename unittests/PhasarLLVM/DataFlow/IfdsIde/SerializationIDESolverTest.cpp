
#include "phasar/DataFlow/IfdsIde/Solver/IDESolver.h"
#include "phasar/DataFlow/IfdsIde/Solver/IFDSSolver.h"
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

    auto TaintProblem2 =
        createAnalysisProblem<IFDSTaintAnalysis>(HA, &Config, EntryPoints);

    IDESolver Solver(TaintProblem, &HA.getICFG());
    auto AtomicResults = Solver.solve();

    // run with interruption
    size_t Counter = 0;
    size_t InterruptionValue = 3;
    std::array<std::string, 4> TempPaths;
    // results with interruption(s)
    auto InterruptedResults = [&] {
      {
        IDESolver Solver(TaintProblem, &HA.getICFG());
        auto Result = Solver.solveUntil(
            [&Counter, InterruptionValue]() {
              if (Counter >= InterruptionValue) {
                return true;
              }
              Counter++;
              return false;
            },
            std::chrono::milliseconds{0});
        EXPECT_EQ(std::nullopt, Result);
        TempPaths = Solver.saveDataInJSONs();

        llvm::outs() << "TempPaths:\n";
        llvm::outs() << "[0]: " << TempPaths[0] << "\n";
        llvm::outs() << "[1]: " << TempPaths[1] << "\n";
        llvm::outs() << "[2]: " << TempPaths[2] << "\n";
        llvm::outs() << "[3]: " << TempPaths[3] << "\n";
        llvm::outs().flush();
      }

      IDESolver Solver(TaintProblem2, &HA.getICFG());

      Solver.loadDataFromJSONs(HA.getProjectIRDB(), TempPaths);

      return std::move(Solver).continueSolving();
    }();

    for (auto &&Cell : AtomicResults.getAllResultEntries()) {
      auto InteractiveRes =
          InterruptedResults.resultAt(Cell.getRowKey(), Cell.getColumnKey());
      EXPECT_EQ(InteractiveRes, Cell.getValue());
    }
  }
}; // Test Fixture

static constexpr std::string_view ISSTestFiles[] = {
    "dummy_source_sink/taint_01_cpp_dbg.ll",
    "dummy_source_sink/taint_02_cpp_dbg.ll",
    "dummy_source_sink/taint_03_cpp_dbg.ll",
    "dummy_source_sink/taint_04_cpp_dbg.ll",
    "dummy_source_sink/taint_05_cpp_dbg.ll",
    "taint_1_cpp.ll",
    "taint_2_cpp.ll",
    "taint_3_cpp.ll",
    "taint_4_cpp.ll",
    "taint_5_cpp.ll",
    "taint_6_cpp.ll",
    "taint_7_cpp.ll",
    "taint_8_cpp.ll",
    "taint_14_cpp.ll",
    "taint_15_cpp.ll",
    "../xtaint/xtaint01_cpp.ll",
    "../xtaint/xtaint02_cpp.ll",
    "../xtaint/xtaint03_cpp.ll",
    "../xtaint/xtaint04_cpp.ll",
    "../xtaint/xtaint05_cpp.ll",
    "../linear_constant/basic_01_cpp_dbg.ll",
    "../linear_constant/basic_02_cpp_dbg.ll",
    "../linear_constant/basic_03_cpp_dbg.ll",
    "../linear_constant/basic_04_cpp_dbg.ll",
    "../linear_constant/basic_05_cpp_dbg.ll",

    "../linear_constant/branch_01_cpp_dbg.ll",
    "../linear_constant/branch_02_cpp_dbg.ll",
    "../linear_constant/branch_03_cpp_dbg.ll",
    "../linear_constant/branch_04_cpp_dbg.ll",
    "../linear_constant/branch_05_cpp_dbg.ll",
    "../linear_constant/branch_06_cpp_dbg.ll",
    "../linear_constant/branch_07_cpp_dbg.ll",

    "../linear_constant/while_01_cpp_dbg.ll",
    "../linear_constant/while_02_cpp_dbg.ll",
    "../linear_constant/while_03_cpp_dbg.ll",
    "../linear_constant/while_04_cpp_dbg.ll",
    "../linear_constant/while_05_cpp_dbg.ll",
    "../linear_constant/for_01_cpp_dbg.ll",

    "../linear_constant/call_01_cpp_dbg.ll",
    "../linear_constant/call_02_cpp_dbg.ll",
    "../linear_constant/call_03_cpp_dbg.ll",
    "../linear_constant/call_04_cpp_dbg.ll",
    "../linear_constant/call_05_cpp_dbg.ll",
    "../linear_constant/call_06_cpp_dbg.ll",
    "../linear_constant/call_07_cpp_dbg.ll",
    "../linear_constant/call_08_cpp_dbg.ll",
    "../linear_constant/call_09_cpp_dbg.ll",
    "../linear_constant/call_10_cpp_dbg.ll",
    "../linear_constant/call_11_cpp_dbg.ll",

    "../linear_constant/recursion_01_cpp_dbg.ll",
    "../linear_constant/recursion_02_cpp_dbg.ll",
    "../linear_constant/recursion_03_cpp_dbg.ll"};

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
