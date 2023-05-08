#ifndef PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H
#define PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H

#include "phasar/Domain/AnalysisDomain.h"
#include "phasar/Domain/LatticeDomain.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/ExtendedTaintAnalysis/AbstractMemoryLocation.h"
#include "phasar/PhasarLLVM/Utils/DataFlowAnalysisType.h"
#include "phasar/Utils/Printer.h"

#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <ostream>
#include <vector>

namespace psr {

template <typename AnalysisTemplate> struct Warnings {

  using n_t = typename AnalysisTemplate::n_t;
  using d_t = typename AnalysisTemplate::d_t;
  using l_t = typename AnalysisTemplate::l_t;

  n_t Instr;
  d_t Fact;
  l_t LatticeElement;
};

template <typename AnalysisTemplate> struct Results {
  DataFlowAnalysisType Analysis;
  std::vector<Warnings<AnalysisTemplate>> War;
};

template <typename AnalysisTemplate> class AnalysisPrinter {
private:
  Results<AnalysisTemplate> Results;

public:
  virtual ~AnalysisPrinter() = default;
  AnalysisPrinter(::psr::Results<AnalysisTemplate> Res) : Results(Res) {}
  void getAnalysisResults(::psr::Results<AnalysisTemplate> Res);
  virtual void emitAnalysisResults(llvm::raw_ostream &OS = llvm::outs()) {

    // TODO: Switch the implementation based on Analysis ?

    for (auto Iter = Results.War.begin(); Iter != Results.War.end(); Iter++) {
      if (Iter->Instr != NULL) {
        OS << "At IR statement: " << llvmIRToString(Iter->Instr) << "\n";
      }
      if (Iter->Fact != NULL) {
        OS << "Fact: " << llvmIRToShortString(Iter->Fact);
      }
      if (Iter->LatticeElement != 0) {
        OS << "\n Value: " << Iter->LatticeElement << "\n";
      }
    }
  }
};

} // namespace psr

#endif