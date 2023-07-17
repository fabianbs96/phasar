#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/Utils/AnalysisPrinter.h"
#include "phasar/PhasarLLVM/Utils/DataFlowAnalysisType.h"

using namespace psr;
// TODO: make it singleton
class NullAnalysisPrinter
    : public AnalysisPrinter<IDELinearConstantAnalysis::n_t,
                             IDELinearConstantAnalysis::d_t,
                             IDELinearConstantAnalysis::l_t> {
public:
  static NullAnalysisPrinter getInstance() {
    static auto Instance = NullAnalysisPrinter();
    return Instance;
  }

  void onInitialize() override{};
  void onResult(
      Warnings<IDELinearConstantAnalysis::n_t, IDELinearConstantAnalysis::d_t,
               IDELinearConstantAnalysis::l_t>
          War) override{};
  void onFinalize(llvm::raw_ostream &OS = llvm::outs()) const override{};

private:
  NullAnalysisPrinter() = default;
};