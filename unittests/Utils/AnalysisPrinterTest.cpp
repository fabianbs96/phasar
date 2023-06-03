#include "phasar/PhasarLLVM/Utils/AnalysisPrinter.h"

#include "phasar/DataFlow/IfdsIde/Solver/IDESolver.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/HelperAnalyses.h"
#include "phasar/PhasarLLVM/SimpleAnalysisConstructor.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include "TestConfig.h"
#include "gtest/gtest.h"
#include "llvm-14/llvm/IR/Instruction.h"

#include <regex>

using namespace psr;

class AnalysisPrinterTest : public ::testing::Test {
protected:
  static constexpr auto PathToLlFiles =
      PHASAR_BUILD_SUBFOLDER("linear_constant/");
  const std::vector<std::string> EntryPoints = {"main"};

  std::string doAnalysis(llvm::StringRef LlvmFilePath) {
    std::string Results;
    llvm::raw_string_ostream Res(Results);
    HelperAnalyses HA(PathToLlFiles + LlvmFilePath, EntryPoints);
    // Compute the ICFG to possibly create the runtime model
    auto &ICFG = HA.getICFG();
    auto HasGlobalCtor = HA.getProjectIRDB().getFunctionDefinition(
                             LLVMBasedICFG::GlobalCRuntimeModelName) != nullptr;

    auto LCAProblem = createAnalysisProblem<IDELinearConstantAnalysis>(
        HA,
        std::vector{HasGlobalCtor ? LLVMBasedICFG::GlobalCRuntimeModelName.str()
                                  : "main"});
    IDESolver LCASolver(LCAProblem, &ICFG);
    LCASolver.solve();
    LCASolver.emitTextReport(Res);
    return Res.str();
  }

  std::string findSubstring(const std::string &Results,
                            const std::string &Pattern) {
    size_t FoundPos = Results.find(Pattern);
    if (FoundPos != std::string::npos) {
      return Pattern;
    }
    return "";
  }

  void checkResults(llvm::StringRef LlvmFilePath, const std::string &Results) {
    psr::HelperAnalyses HA(PathToLlFiles + LlvmFilePath, EntryPoints);

    // Test Instruction ID:5
    std::string GroundTruth =
        llvmIRToString(HA.getProjectIRDB().getInstruction(5));
    std::regex Pattern(R"(, !psr\.id !\d+ \| ID: \d+)");
    std::string TrimmedGroundTruth =
        std::regex_replace(GroundTruth, Pattern, "");
    std::string TrimmedResults = findSubstring(Results, TrimmedGroundTruth);

    EXPECT_EQ(TrimmedGroundTruth, TrimmedResults);
  }
};

/* ============== BASIC TESTS ============== */
TEST_F(AnalysisPrinterTest, HandleBasicTest_01) {
  auto Results = doAnalysis("simple_cpp.ll");
  checkResults("simple_cpp.ll", Results);
}

// main function for the test case
int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}