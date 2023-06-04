
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IFDSUninitializedVariables.h"

#include "phasar/DataFlow/IfdsIde/Solver/IFDSSolver.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/HelperAnalyses.h"
#include "phasar/PhasarLLVM/Passes/ValueAnnotationPass.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/SimpleAnalysisConstructor.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h"

#include "llvm/IR/Module.h"

#include "TestConfig.h"
#include "gtest/gtest.h"

#include <memory>
#include <optional>

using namespace std;
using namespace psr;

/* ============== TEST FIXTURE ============== */

class IFDSUninitializedVariablesTest : public ::testing::Test {
protected:
  static constexpr auto PathToLlFiles =
      PHASAR_BUILD_SUBFOLDER("uninitialized_variables/");
  const std::vector<std::string> EntryPoints = {"main"};

  std::optional<HelperAnalyses> HA;

  std::optional<IFDSUninitializedVariables> UninitProblem;

  IFDSUninitializedVariablesTest() = default;

  void initialize(const llvm::Twine &IRFile) {
    HA.emplace(IRFile, EntryPoints);
    UninitProblem =
        createAnalysisProblem<IFDSUninitializedVariables>(*HA, EntryPoints);
  }

  void doAnalysis(const llvm::Twine &IRFile,
                  const map<int, set<string>> &GroundTruth,
                  bool DumpResults = false) {
    HelperAnalyses HA(PathToLlFiles + IRFile, EntryPoints);
    auto UninitProblem =
        createAnalysisProblem<IFDSUninitializedVariables>(HA, EntryPoints);

    {
      IFDSSolver Solver(UninitProblem, &HA.getICFG());
      Solver.solve();

      if (DumpResults) {
        Solver.dumpResults();
      }
    }

    compareResults(UninitProblem, GroundTruth);
  }

  void SetUp() override { ValueAnnotationPass::resetValueID(); }

  void TearDown() override {}

  static void compareResults(const IFDSUninitializedVariables &UninitProblem,
                             const map<int, set<string>> &GroundTruth) {

    map<int, set<string>> FoundUninitUses;
    for (const auto &Kvp : UninitProblem.getAllUndefUses()) {
      auto InstID = stoi(getMetaDataID(Kvp.first));
      set<string> UndefValueIds;
      for (const auto *UV : Kvp.second) {
        UndefValueIds.insert(getMetaDataID(UV));
      }
      FoundUninitUses[InstID] = UndefValueIds;
    }

    EXPECT_EQ(FoundUninitUses, GroundTruth);
  }

  void compareResults(map<int, set<string>> &GroundTruth) {
    compareResults(*UninitProblem, GroundTruth);
  }
}; // Test Fixture

TEST_F(IFDSUninitializedVariablesTest, UninitTest_01_SHOULD_NOT_LEAK) {
  // all_uninit.cpp does not contain undef-uses
  doAnalysis("all_uninit_cpp_dbg.ll", {});
}

TEST_F(IFDSUninitializedVariablesTest, UninitTest_02_SHOULD_LEAK) {
  // binop_uninit uses uninitialized variable i in 'int j = i + 10;'
  doAnalysis("binop_uninit_cpp_dbg.ll", {
                                            {6, {"1"}},
                                        });

  // %4 = load i32, i32* %2, ID: 6 ;  %2 is the uninitialized variable i
}

TEST_F(IFDSUninitializedVariablesTest, UninitTest_03_SHOULD_LEAK) {
  // callnoret uses uninitialized variable a in 'return a + 10;' of addTen(int)
  doAnalysis("callnoret_c_dbg.ll",
             {
                 // %4 = load i32, i32* %2 ; %2 is the parameter a of
                 // addTen(int) containing undef
                 {5, {"0"}},
                 // %5 = load i32, i32* %2 ; %2 is the uninitialized variable a
                 {16, {"9"}},
             });
}

TEST_F(IFDSUninitializedVariablesTest, UninitTest_04_SHOULD_NOT_LEAK) {
  doAnalysis("ctor_default_cpp_dbg.ll", {});
}

TEST_F(IFDSUninitializedVariablesTest, UninitTest_05_SHOULD_NOT_LEAK) {
  doAnalysis("struct_member_init_cpp_dbg.ll", {});
}

TEST_F(IFDSUninitializedVariablesTest, UninitTest_06_SHOULD_NOT_LEAK) {
  doAnalysis("struct_member_uninit_cpp_dbg.ll", {});
}
/****************************************************************************************
 * fails due to field-insensitivity + struct ignorance + clang compiler hacks
 *
 *****************************************************************************************/
TEST_F(IFDSUninitializedVariablesTest, UninitTest_07_SHOULD_LEAK) {
  doAnalysis("struct_member_uninit2_cpp_dbg.ll", {{4, {"3"}}});
}
TEST_F(IFDSUninitializedVariablesTest, UninitTest_08_SHOULD_NOT_LEAK) {
  doAnalysis("global_variable_cpp_dbg.ll", {});
}
/****************************************************************************************
 * fails since @i is uninitialized in the c++ code, but initialized in the
 * LLVM-IR
 *
 *****************************************************************************************/
TEST_F(IFDSUninitializedVariablesTest, UninitTest_09_SHOULD_LEAK) {
  doAnalysis("global_variable_cpp_dbg.ll", {{5, {"0"}}});
}
TEST_F(IFDSUninitializedVariablesTest, UninitTest_10_SHOULD_LEAK) {
  // What about this call?
  // %3 = call i32 @_Z3foov()
  // GroundTruth[8] = {""};
  doAnalysis("return_uninit_cpp_dbg.ll", {{2, {"0"}}});
}

TEST_F(IFDSUninitializedVariablesTest, UninitTest_11_SHOULD_NOT_LEAK) {
  // all undef-uses are sanitized;
  // However, the uninitialized variable j is read, which causes the analysis to
  // report an undef-use
  doAnalysis("sanitizer_cpp_dbg.ll", {{6, {"2"}}});
}
//---------------------------------------------------------------------
// Not relevant any more; Test case covered by UninitTest_11
//---------------------------------------------------------------------
TEST_F(IFDSUninitializedVariablesTest, UninitTest_12_SHOULD_LEAK) {
  doAnalysis("sanitizer_uninit_cpp_dbg.ll", {{6, {"2"}}, {13, {"2"}}});
}
TEST_F(IFDSUninitializedVariablesTest, UninitTest_13_SHOULD_NOT_LEAK) {
  // The undef-uses do not affect the program behaviour, but are of course still
  // found and reported
  doAnalysis("sanitizer2_cpp_dbg.ll", {{9, {"2"}}});
}
TEST_F(IFDSUninitializedVariablesTest, UninitTest_14_SHOULD_LEAK) {
  doAnalysis("uninit_c_dbg.ll", {{14, {"1"}}, {15, {"2"}}, {16, {"14", "15"}}});
}
/****************************************************************************************
 * Fails probably due to field-insensitivity
 *
 *****************************************************************************************/
TEST_F(IFDSUninitializedVariablesTest, UninitTest_15_SHOULD_LEAK) {
  // TODO remove GT[14] and GT[13]
  // Analysis detects false positive at %12:

  // store i32* %3, i32** %6, align 8, !dbg !28
  // %12 = load i32*, i32** %6, align 8, !dbg !29
  doAnalysis("dyn_mem_cpp_dbg.ll", {{13, {"2"}},
                                    {14, {"3"}},
                                    {28, {"2"}},
                                    {29, {"3"}},
                                    {30, {"28", "29"}},
                                    {33, {"30"}},
                                    {35, {"4"}},
                                    {38, {"35"}}});
}
TEST_F(IFDSUninitializedVariablesTest, UninitTest_16_SHOULD_LEAK) {
  doAnalysis("growing_example_cpp_dbg.ll",
             {{11, {"0"}}, {16, {"2"}}, {18, {"16"}}, {34, {"24"}}});
}

/****************************************************************************************
 * Fails due to struct ignorance; general problem with field sensitivity: when
 * all structs would be treated as uninitialized per default, the analysis would
 * not be able to detect correct constructor calls
 *
 *****************************************************************************************/
TEST_F(IFDSUninitializedVariablesTest, UninitTest_17_SHOULD_LEAK) {
  doAnalysis("struct_test_cpp_dbg.ll", {{8, {"5", "7"}}});
}
/****************************************************************************************
 * Fails, since the analysis is not able to detect memcpy calls
 *
 *****************************************************************************************/
TEST_F(IFDSUninitializedVariablesTest, UninitTest_18_SHOULD_NOT_LEAK) {
  doAnalysis("array_init_cpp_dbg.ll", {});
}
/****************************************************************************************
 * fails due to missing alias information (and missing field/array element
 *information)
 *
 *****************************************************************************************/
TEST_F(IFDSUninitializedVariablesTest, UninitTest_19_SHOULD_NOT_LEAK) {
  doAnalysis("array_init_simple_cpp_dbg.ll", {});
}
TEST_F(IFDSUninitializedVariablesTest, UninitTest_20_SHOULD_LEAK) {
  doAnalysis(
      "recursion_cpp_dbg.ll",
      {{11, {"2"}}, {14, {"2"}}, {31, {"24"}}, {20, {"1"}}, {29, {"28"}}});
}
TEST_F(IFDSUninitializedVariablesTest, UninitTest_21_SHOULD_LEAK) {
  doAnalysis("virtual_call_cpp_dbg.ll",
             {{3, {"0"}}, {8, {"5"}}, {10, {"5"}}, {35, {"34"}}, {37, {"17"}}});
}
TEST_F(IFDSUninitializedVariablesTest, UninitTest_22_SHOULD_LEAK) {
  doAnalysis(
      "test_test_uninit_cpp_dbg.ll",
      {{0, {"0"}}, {2, {"4"}}, {3, {"0"}}, {4, {"0"}}, {5, {"0"}}, {7, {"8"}}});
}
TEST_F(IFDSUninitializedVariablesTest, UninitTest_23_SHOULD_LEAK) {
  doAnalysis("reference_uninit_cpp_dbg.ll", {{0, {"0"}}, {2, {"0"}}});
}
int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
