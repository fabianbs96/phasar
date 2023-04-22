#include "phasar/Domain/AnalysisDomain.h"
#include "phasar/Domain/LatticeDomain.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include <vector>

namespace psr{

//TODO: use pre-defined enums
enum AnalysisType {LCA, TSA, TA};

//TODO: use generic types n_t, d_t templates instead of llvm
struct Warnings{
    using n_t = typename AnalysisDomain::n_t;
    using d_t = typename AnalysisDomain::d_t;
    using l_t = typename AnalysisDomain::l_t; // TODO: v_t can be used?
    n_t *Instr;
    d_t *Fact;
    l_t *LatticeElement;
    //llvm::Instruction* Instr;llvm::Value* Value;psr::LLVMProjectIRDB LatticeElement ;
};

template <typename AnalysisDomain>
struct Results{ // TODO: whats the issue here
    AnalysisType Analysis;
    std::vector<Warnings> War;
};

class AnalysisPrinter{
private:
  Results<AnalysisDomain> Results;
public:
  virtual ~AnalysisPrinter() = default;
  AnalysisPrinter(psr::Results<AnalysisDomain> Res){
    Res = Results;
  }
  void getAnalysisResults(psr::Results<AnalysisDomain> Res);
  virtual void emitAnalysisResults();
};

} // namespace psr
