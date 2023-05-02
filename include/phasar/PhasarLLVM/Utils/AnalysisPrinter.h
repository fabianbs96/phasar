#include "phasar/Domain/AnalysisDomain.h"
#include "phasar/Domain/LatticeDomain.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/ExtendedTaintAnalysis/AbstractMemoryLocation.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Printer.h"

#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include <cstddef>
#include <ostream>
#include <vector>

namespace psr {

// TODO: use pre-defined enums
enum AnalysisType { LCA, TSA, TA };

template <typename AnalysisTemplate> struct Warnings {
  using n_t = typename AnalysisTemplate::n_t;
  using d_t = typename AnalysisTemplate::d_t;
  using l_t = typename AnalysisTemplate::l_t;
  using v_t = typename AnalysisTemplate::v_t;

  n_t Instr;
  d_t Fact;
  l_t LatticeElement;
  v_t Value;
};

template <typename AnalysisTemplate>
struct Results { // TODO: whats the issue here
  AnalysisType Analysis;
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
        OS << "\n Instr is " << llvmIRToString(Iter->Instr) << "\n";
      }
      if (Iter->Fact != NULL) {
        OS << "Data Flow Fact" << llvmIRToShortString(Iter->Fact) << "\n";
      }
      if (Iter->LatticeElement != 0) {
        OS << "Lattice Value " << Iter->LatticeElement << "\n";
      }
      if (Iter->Value != NULL) {
        OS << "Value is " << Iter->Value << "\n";
      }
    }
  }
};

} // namespace psr
