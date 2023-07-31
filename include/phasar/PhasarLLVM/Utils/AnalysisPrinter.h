#ifndef PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H
#define PHASAR_PHASARLLVM_UTILS_ANALYSISPRINTER_H

#include "phasar/Domain/AnalysisDomain.h"
#include "phasar/Domain/BinaryDomain.h"
#include "phasar/Domain/LatticeDomain.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Domain/LLVMAnalysisDomain.h"
#include "phasar/PhasarLLVM/Utils/DataFlowAnalysisType.h"
#include "phasar/PhasarLLVM/Utils/LLVMIRToSrc.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Printer.h"

#include "llvm/ADT/None.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <iostream>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

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
template <typename AnalysisDomainTy>
class AnalysisPrinter : public NodePrinter<AnalysisDomainTy>,
                        public DataFlowFactPrinter<AnalysisDomainTy>,
                        public EdgeFactPrinter<AnalysisDomainTy> {
public:
  virtual ~AnalysisPrinter() = default;
  AnalysisPrinter() : AnalysisResults{.War = {}} {}
  virtual void onResult(Warnings<AnalysisDomainTy> War) {
    AnalysisResults.War.emplace_back(std::move(War));
  }
  virtual void onInitialize(){};
  virtual void onFinalize(llvm::raw_ostream &OS = llvm::outs()) const {

    for (auto Iter : AnalysisResults.War) {

      OS << "\nAt IR statement: " << this->NtoString(Iter.Instr) << "\n";

      OS << "Fact: " << this->DtoString(Iter.Fact) << "\n";

      if constexpr (std::is_same_v<typename AnalysisDomainTy::l_t,
                                   BinaryDomain>) {
        OS << "Value: " << this->LtoString(Iter.LatticeElement) << "\n";
      }
    }
  }

  void printNode(llvm::raw_ostream &OS,
                 typename AnalysisDomainTy::n_t Stmt) const override;

  void printDataFlowFact(llvm::raw_ostream &OS,
                         typename AnalysisDomainTy::d_t Fact) const override;

  void printEdgeFact(llvm::raw_ostream &OS,
                     typename AnalysisDomainTy::l_t L) const override;

private:
  Results<AnalysisDomainTy> AnalysisResults;
};

} // namespace psr

#endif
