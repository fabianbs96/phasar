#ifndef PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H
#define PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H

#include "phasar/Domain/BinaryDomain.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include <optional>
#include <type_traits>
#include <vector>

namespace psr {

template <typename AnalysisDomainTy> struct Warnings {
  typename AnalysisDomainTy::n_t Instr;
  typename AnalysisDomainTy::d_t Fact;
  typename AnalysisDomainTy::l_t LatticeElement;

  // Constructor
  Warnings(typename AnalysisDomainTy::n_t Inst,
           typename AnalysisDomainTy::d_t DfFact,
           typename AnalysisDomainTy::l_t Lattice)
      : Instr(std::move(Inst)), Fact(std::move(DfFact)),
        LatticeElement(std::move(Lattice)) {}
};

template <typename AnalysisDomainTy> struct Results {
  std::vector<Warnings<AnalysisDomainTy>> War;
};

// TODO: explicit-template-instantiation
template <typename AnalysisDomainTy> class AnalysisPrinter {
public:
  virtual ~AnalysisPrinter() = default;
  AnalysisPrinter() : AnalysisResults{.War = {}} {}

  AnalysisPrinter(const AnalysisPrinter &) = delete;
  AnalysisPrinter &operator=(const AnalysisPrinter &) = delete;

  AnalysisPrinter(AnalysisPrinter &&) = delete;
  AnalysisPrinter &operator=(AnalysisPrinter &&) = delete;

  virtual void onResult(Warnings<AnalysisDomainTy> War) {
    AnalysisResults.War.emplace_back(std::move(War));
  }

  virtual void onInitialize(){};
  virtual void onFinalize(llvm::raw_ostream &OS = llvm::outs()) const {
    for (auto Iter : AnalysisResults.War) {

      OS << "\nAt IR statement: " << psr::nToString(Iter.Instr) << "\n";

      OS << "Fact: " << psr::dToString(Iter.Fact) << "\n";

      if constexpr (std::is_same_v<typename AnalysisDomainTy::l_t,
                                   BinaryDomain>) {
        OS << "Value: " << psr::lToString(Iter.LatticeElement) << "\n";
      }
    }
  }

private:
  Results<AnalysisDomainTy> AnalysisResults;
};

} // namespace psr

#endif
