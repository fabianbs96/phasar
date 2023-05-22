#ifndef PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H
#define PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H

#include "phasar/Domain/AnalysisDomain.h"
#include "phasar/Domain/LatticeDomain.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/ExtendedTaintAnalysis/AbstractMemoryLocation.h"
#include "phasar/PhasarLLVM/Utils/DataFlowAnalysisType.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Printer.h"

#include "llvm/ADT/None.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <ostream>
#include <type_traits>
#include <vector>

namespace psr {

template <typename n_t, typename d_t, typename l_t> struct Warnings {
  n_t Instr;
  d_t Fact;
  l_t LatticeElement;

  // Constructor
  Warnings(n_t Inst, d_t DfFact, l_t Lattice)
      : Instr(Inst), Fact(DfFact), LatticeElement(Lattice) {}
};

template <typename n_t, typename d_t, typename l_t> struct Results {
  DataFlowAnalysisType AnalysisType;
  std::vector<Warnings<n_t, d_t, l_t>> War;
};

template <typename n_t, typename d_t, typename l_t, bool LatticePrinter = false>
class AnalysisPrinter {
private:
  const Results<n_t, d_t, l_t> &Results;

public:
  virtual ~AnalysisPrinter() = default;
  AnalysisPrinter(const ::psr::Results<n_t, d_t, l_t> &Res) : Results(Res) {}

  virtual void emitAnalysisResults(llvm::raw_ostream &OS = llvm::outs()) {

    OS << "\n===== " << Results.AnalysisType << " =====\n";

    for (auto Iter = Results.War.begin(); Iter != Results.War.end(); Iter++) {
      if (Iter->Instr) {
        OS << "\nAt IR statement: " << llvmIRToString(Iter->Instr) << "\n";
      }
      if (Iter->Fact) {
        OS << "Fact: " << llvmIRToShortString(Iter->Fact) << "\n";
      }

      if constexpr (LatticePrinter) {
        OS << "Value: " << Iter->LatticeElement << "\n";
      }
    }
  }
};

} // namespace psr

#endif