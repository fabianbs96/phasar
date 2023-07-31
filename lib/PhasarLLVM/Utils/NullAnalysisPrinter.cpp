#include "phasar/PhasarLLVM/Utils/NullAnalysisPrinter.h"

namespace psr {

template <typename AnalysisDomainTy>
void NullAnalysisPrinter<AnalysisDomainTy>::onInitialize() {}

template <typename AnalysisDomainTy>
void NullAnalysisPrinter<AnalysisDomainTy>::onResult(
    Warnings<AnalysisDomainTy> /*War*/) {}

template <typename AnalysisDomainTy>
void NullAnalysisPrinter<AnalysisDomainTy>::onFinalize(
    llvm::raw_ostream & /*OS*/) {}
} // namespace psr