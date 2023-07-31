#include "phasar/PhasarLLVM/Utils/AnalysisPrinter.h"

namespace psr {

template <typename AnalysisDomainTy>
class NullAnalysisPrinter : public AnalysisPrinter<AnalysisDomainTy> {
public:
  static const NullAnalysisPrinter *getInstance() {
    static auto Instance = NullAnalysisPrinter();
    return &Instance;
  }

  void onInitialize() override;
  void onResult(Warnings<AnalysisDomainTy> /*War*/) override;
  void onFinalize(llvm::raw_ostream & /*OS*/) override;

private:
  NullAnalysisPrinter() = default;
};

} // namespace psr