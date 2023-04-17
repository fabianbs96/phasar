#include "phasar/Domain/AnalysisDomain.h"
#include "phasar/Domain/LatticeDomain.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include <vector>

namespace psr{

//TODO: use pre-defined enums
enum AnalysisType {LCA, TSA, TA};

//TODO: use generic types n_t, d_t templates instead of llvm
template <typename AnalysisTemplate>
struct Warnings : public AnalysisDomain{
    using n_t = const typename AnalysisTemplate::n_t;
    using d_t = const typename AnalysisTemplate::d_t;
    using l_t = const typename AnalysisTemplate::l_t; // TODO: v_t can be used?
    n_t *Instr;
    d_t *Fact;
    l_t *LatticeElement;
    //llvm::Instruction* Instr;llvm::Value* Value;psr::LLVMProjectIRDB LatticeElement ;
};


struct Results{ // TODO: whats the issue here
    AnalysisType Analysis;
    std::vector<Warnings> War;
};

class AnalysisPrinter{
private:
  Results Results;
public:
  virtual ~AnalysisPrinter() = default;
  AnalysisPrinter(struct Results Res){
    Results = Res;
  }
  void getAnalysisResults(struct Results Res);
  virtual void emitAnalysisResults();
};

} // namespace psr
