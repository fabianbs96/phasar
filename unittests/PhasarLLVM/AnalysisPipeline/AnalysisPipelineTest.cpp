#include "phasar/PhasarLLVM/AnalysisPipeline.h"

#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IFDSUninitializedVariables.h"

#include "SrcCodeLocationEntry.h"
#include "TestConfig.h"
#include "gtest/gtest.h"

#include <map>
#include <set>
#include <type_traits>

using namespace psr;
using namespace psr::unittest;

namespace {

constexpr auto PathToLlFiles =
    PHASAR_BUILD_SUBFOLDER("uninitialized_variables/");

using GroundTruthTy =
    std::map<TestingSrcLocation, std::set<TestingSrcLocation>>;

// Ground truth for binop_uninit.cpp, shared with
// IFDSUninitializedVariablesTest.UninitTest_02_SHOULD_LEAK.
GroundTruthTy binopUninitGroundTruth() {
  const auto Entry = LineColFun{2, 0, "main"};
  const auto EntryTwo = LineColFun{3, 11, "main"};
  const auto EntryThree = LineColFun{3, 13, "main"};
  GroundTruthTy GroundTruth;
  GroundTruth.insert({EntryTwo, {Entry}});
  GroundTruth.insert({EntryThree, {EntryTwo}});
  return GroundTruth;
}

struct DummyTag {};
struct DummyStageA {
  using tag_t = DummyTag;
  using result_t = int;
  static auto build(auto & /*unused*/) { return 1; }
};
struct DummyStageB {
  using tag_t = DummyTag;
  using result_t = int;
  static auto build(auto & /*unused*/) { return 2; }
};

} // namespace

TEST(AnalysisPipelineTest, BuildsAndSolvesIFDSProblem) {
  auto Pipeline = defaultPipeline(PathToLlFiles + "binop_uninit_cpp_dbg.ll")
                      .with(DataflowAnalysisStage{
                          std::type_identity<IFDSUninitializedVariables>{}})
                      .shared();

  Pipeline.solve();

  auto &Problem = Pipeline.getResult(DataflowAnalysisTag{});
  auto &IRDB = Pipeline.getResult(IRDBTag{});
  auto ConvGroundTruth =
      convertTestingLocationSetMapInIR(binopUninitGroundTruth(), IRDB);

  EXPECT_EQ(Problem.getAllUndefUses(), ConvGroundTruth);
}

// defaultPipeline() runs ICFGStage twice (RTA, then VTA), both tagged
// ICFGTag. Reproduced here with dummy stages instead of a real ICFG, since
// RTA and VTA happen to agree on this tiny fixture and would not otherwise
// catch a regression to "first match wins".
TEST(AnalysisPipelineTest, DuplicateTagResolvesToLatestStage) {
  auto P = Pipeline<>{}.withValue(DummyStageA{}, 1).with(DummyStageB{});
  EXPECT_EQ(P.getResult(DummyTag{}), 2);
}

// .shared() lets several continuations branch off one already-built prefix
// instead of rebuilding it; check both branches see the same prefix objects
// and still solve correctly.
TEST(AnalysisPipelineTest, SharedPipelineBranchesShareTheirPrefix) {
  auto Shared =
      defaultPipeline(PathToLlFiles + "binop_uninit_cpp_dbg.ll").shared();

  auto Branch1 = Shared.with(
      DataflowAnalysisStage{std::type_identity<IFDSUninitializedVariables>{}});
  auto Branch2 = Shared.with(
      DataflowAnalysisStage{std::type_identity<IFDSUninitializedVariables>{}});

  EXPECT_EQ(&Branch1.getResult(ICFGTag{}), &Shared.getResult(ICFGTag{}));
  EXPECT_EQ(&Branch2.getResult(ICFGTag{}), &Shared.getResult(ICFGTag{}));

  std::move(Branch1).solve();
  std::move(Branch2).solve();

  auto &IRDB = Shared.getResult(IRDBTag{});
  auto ConvGroundTruth =
      convertTestingLocationSetMapInIR(binopUninitGroundTruth(), IRDB);

  EXPECT_EQ(Branch1.getResult(DataflowAnalysisTag{}).getAllUndefUses(),
            ConvGroundTruth);
  EXPECT_EQ(Branch2.getResult(DataflowAnalysisTag{}).getAllUndefUses(),
            ConvGroundTruth);
}

int main(int Argc, char **Argv) {
  ::testing::InitGoogleTest(&Argc, Argv);
  return RUN_ALL_TESTS();
}
