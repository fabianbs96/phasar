#ifndef PHASAR_PHASARLLVM_UTILS_DEFAULTANALYSISPRINTER_H
#define PHASAR_PHASARLLVM_UTILS_DEFAULTANALYSISPRINTER_H

#include "phasar/Domain/BinaryDomain.h"
#include "phasar/PhasarLLVM/Utils/AnalysisPrinterBase.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include <optional>
#include <type_traits>
#include <vector>

namespace psr {

template <typename AnalysisDomainTy>
class DefaultAnalysisPrinter : public AnalysisPrinterBase<AnalysisDomainTy> {
  using l_t = typename AnalysisDomainTy::l_t;

public:
  virtual ~DefaultAnalysisPrinter() = default;
  DefaultAnalysisPrinter() = default;

  DefaultAnalysisPrinter(const DefaultAnalysisPrinter &) = delete;
  DefaultAnalysisPrinter &operator=(const DefaultAnalysisPrinter &) = delete;

  DefaultAnalysisPrinter(DefaultAnalysisPrinter &&) = delete;
  DefaultAnalysisPrinter &operator=(DefaultAnalysisPrinter &&) = delete;

  void onResult(Warnings<AnalysisDomainTy> War) override {
    AnalysisResults.War.emplace_back(std::move(War));
  }

  void onInitialize() override{};
  void onFinalize(llvm::raw_ostream &OS = llvm::outs()) const override {
    for (auto Iter : AnalysisResults.War) {

      OS << "\nAt IR statement: " << psr::nToString(Iter.Instr) << "\n";

      OS << "\tFact: " << psr::dToString(Iter.Fact) << "\n";

      if constexpr (std::is_same_v<l_t, BinaryDomain>) {
        OS << "Value: " << psr::lToString(Iter.LatticeElement) << "\n";
      }
    }
  }

private:
  Results<AnalysisDomainTy> AnalysisResults{};
};

} // namespace psr

#endif
