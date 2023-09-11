#ifndef PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTERBASE_H
#define PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTERBASE_H

#include <llvm/Support/raw_ostream.h>

namespace psr {

template <typename AnalysisDomainTy> struct Warnings {
  using n_t = typename AnalysisDomainTy::n_t;
  using d_t = typename AnalysisDomainTy::d_t;
  using l_t = typename AnalysisDomainTy::l_t;

  n_t Instr;
  d_t Fact;
  l_t LatticeElement;

  // Constructor
  Warnings(n_t Inst, d_t DfFact, l_t Lattice)
      : Instr(std::move(Inst)), Fact(std::move(DfFact)),
        LatticeElement(std::move(Lattice)) {}
};

template <typename AnalysisDomainTy> struct Results {
  std::vector<Warnings<AnalysisDomainTy>> War;
};

template <typename AnalysisDomainTy> class AnalysisPrinterBase {
public:
  virtual void onResult(Warnings<AnalysisDomainTy> /*War*/) = 0;
  virtual void onInitialize() = 0;
  virtual void onFinalize(llvm::raw_ostream & /*OS*/) const = 0;

  AnalysisPrinterBase() = default;
};

} // namespace psr

#endif
