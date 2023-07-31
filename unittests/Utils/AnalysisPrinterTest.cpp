#include "phasar/PhasarLLVM/Utils/AnalysisPrinter.h"

#include "phasar/DataFlow/IfdsIde/Solver/IDESolver.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/HelperAnalyses.h"
#include "phasar/PhasarLLVM/SimpleAnalysisConstructor.h"
#include "phasar/PhasarLLVM/Utils/DataFlowAnalysisType.h"
#include "phasar/PhasarLLVM/Utils/LLVMIRToSrc.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include "TestConfig.h"
#include "gtest/gtest.h"

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <ostream>
#include <regex>

#include <nlohmann/json.hpp>

using namespace psr;

// Use template to variate between Typesate and Taint analysis
class GroundTruthCollector
    : public AnalysisPrinter<IDELinearConstantAnalysisDomain> {
public:
  // constructor init Groundtruth in each fixture
  GroundTruthCollector(const std::vector<SourceCodeInfo> &GroundTruth,
                       size_t GroundTruthCount)
      : GroundTruthCount(GroundTruthCount), GroundTruth(GroundTruth){};

  void removeElement(SourceCodeInfo G) {
    auto Iter = std::find(GroundTruth.begin(), GroundTruth.end(), G);
    GroundTruth.erase(Iter);
  }

  void onResult(Warnings<IDELinearConstantAnalysisDomain> War) override {
    for (auto G : GroundTruth) {
      if (G.equivalentWith(getSrcCodeInfoFromIR(War.Fact))) {
        Count++;
        removeElement(G); // make it to llvm::DenseSet instead of std::vector
        break;
      }
    }
  }

  void onFinalize(llvm::raw_ostream &OS = llvm::outs()) const override {
    ASSERT_NE(GroundTruthCount, 0);
    EXPECT_TRUE(Count == GroundTruthCount);
  }

private:
  size_t Count = 0;
  size_t GroundTruthCount = 0;
  std::vector<SourceCodeInfo> GroundTruth;
};

class AnalysisPrinterTest : public ::testing::Test {
protected:
  static constexpr auto PathToLlFiles =
      PHASAR_BUILD_SUBFOLDER("linear_constant/");
  const std::vector<std::string> EntryPoints = {"main"};

  void doAnalysisTest(llvm::StringRef LlvmFilePath,
                      GroundTruthCollector &GroundTruthPrinter) {
    HelperAnalyses HA(PathToLlFiles + LlvmFilePath, EntryPoints);
    // Compute the ICFG to possibly create the runtime model
    auto &ICFG = HA.getICFG();
    auto HasGlobalCtor = HA.getProjectIRDB().getFunctionDefinition(
                             LLVMBasedICFG::GlobalCRuntimeModelName) != nullptr;

    auto LCAProblem = createAnalysisProblem<IDELinearConstantAnalysis>(
        HA,
        std::vector{HasGlobalCtor ? LLVMBasedICFG::GlobalCRuntimeModelName.str()
                                  : "main"});
    LCAProblem.setAnalysisPrinter(&GroundTruthPrinter);
    IDESolver LCASolver(LCAProblem, &ICFG);
    LCASolver.solve();
    LCASolver.emitTextReport();
  }
};

/* ============== BASIC TESTS ============== */

TEST_F(AnalysisPrinterTest, HandleBasicTest_01) {
  std::vector<SourceCodeInfo> GroundTruth = {{.SourceCodeLine = "int i = 4;",
                                              .SourceCodeFunctionName = "main",
                                              .Line = 2,
                                              .Column = 7},
                                             {.SourceCodeLine = "int j = 5;",
                                              .SourceCodeFunctionName = "main",
                                              .Line = 3,
                                              .Column = 7}};
  GroundTruthCollector GroundTruthPrinter = {GroundTruth, GroundTruth.size()};
  doAnalysisTest("simple_cpp_dbg.ll", GroundTruthPrinter);
}

TEST_F(AnalysisPrinterTest, HandleBasicTest_02) {
  std::vector<SourceCodeInfo> GroundTruth = {
      {.SourceCodeLine = "int k = i + j;",
       .SourceCodeFunctionName = "main",
       .Line = 4,
       .Column = 11}};
  GroundTruthCollector GroundTruthPrinter = {GroundTruth, GroundTruth.size()};
  doAnalysisTest("simple_cpp_dbg.ll",
                 GroundTruthPrinter); // TODO: re-use existing files and remove
                                      // this new file
}

/* ============== ERROR TESTS ============== */

TEST_F(AnalysisPrinterTest, HandleBasicTest_03) {
  std::vector<SourceCodeInfo> GroundTruth = {{.SourceCodeLine = "int i = 6;",
                                              .SourceCodeFunctionName = "main",
                                              .Line = 2,
                                              .Column = 7}};
  GroundTruthCollector GroundTruthPrinter = {GroundTruth, GroundTruth.size()};
  doAnalysisTest("simple_cpp_dbg.ll", GroundTruthPrinter);
}

TEST_F(AnalysisPrinterTest, HandleBasicTest_04) {
  std::vector<SourceCodeInfo> GroundTruth = {};
  GroundTruthCollector GroundTruthPrinter = {GroundTruth, GroundTruth.size()};
  doAnalysisTest("simple_cpp_dbg.ll", GroundTruthPrinter);
}

// main function for the test case
int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
