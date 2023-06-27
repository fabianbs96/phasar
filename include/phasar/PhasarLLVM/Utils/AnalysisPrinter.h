#ifndef PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H
#define PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H

#include "phasar/Domain/AnalysisDomain.h"
#include "phasar/Domain/LatticeDomain.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/ExtendedTaintAnalysis/AbstractMemoryLocation.h"
#include "phasar/PhasarLLVM/Utils/DataFlowAnalysisType.h"
#include "phasar/PhasarLLVM/Utils/LLVMIRToSrc.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Printer.h"

#include "llvm/ADT/None.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <iostream>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace psr {

template <typename N_t, typename D_t, typename L_t> struct Warnings {
  N_t Instr;
  D_t Fact;
  L_t LatticeElement;

  // Constructor
  Warnings(N_t Inst, D_t DfFact, L_t Lattice)
      : Instr(std::move(Inst)), Fact(std::move(DfFact)),
        LatticeElement(std::move(Lattice)) {}
};

template <typename N_t, typename D_t, typename L_t> struct Results {
  DataFlowAnalysisType AnalysisType;
  std::vector<Warnings<N_t, D_t, L_t>> War;
};

template <typename N_t, typename D_t, typename L_t, bool LatticePrinter = false>
class AnalysisPrinter {
private:
  Results<N_t, D_t, L_t> AnalysisResults;

public:
  virtual ~AnalysisPrinter() = default;
  AnalysisPrinter(DataFlowAnalysisType AnalysisType)
      : AnalysisResults{.AnalysisType = AnalysisType, .War = {}} {}
  virtual void onResult(Warnings<N_t, D_t, L_t> War) {
    AnalysisResults.War.emplace_back(std::move(War));
  }
  virtual void onInitialize(){};
  virtual void onFinalize(llvm::raw_ostream &OS = llvm::outs()) const {

    OS << "\n===== " << AnalysisResults.AnalysisType << " =====\n";

    for (auto Iter : AnalysisResults.War) {
      if (Iter.Instr) {
        OS << "\nAt IR statement: " << llvmIRToString(Iter.Instr) << "\n";
      }
      if (Iter.Fact) {
        OS << "Fact: " << llvmIRToShortString(Iter.Fact) << "\n";
      }

      if constexpr (LatticePrinter) {
        OS << "Value: " << Iter.LatticeElement << "\n";
      }
    }
  }
};

} // namespace psr

#endif