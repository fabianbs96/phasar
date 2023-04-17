#include "phasar/Domain/AnalysisDomain.h"
#include "phasar/Domain/LatticeDomain.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include <vector>

namespace psr{

//TODO: use pre-defined enums
enum AnalysisType {LCA, TSA, TA};

//TODO: use generic types n_t, d_t templates instead of llvm
struct Warnings{
    using d_t = typename AnalysisDomain::d_t;
    using v_t = typename AnalysisDomain::v_t;
    using l_t = typename AnalysisDomain::l_t;
    //llvm::Instruction* Instr;llvm::Value* Value;psr::LLVMProjectIRDB LatticeElement ;
};

struct Results{
    AnalysisType Analysis;
    std::vector<Warnings> War;
};

//template <typename AnalysisDomainTy,
        //  typename Container = std::set<typename AnalysisDomainTy::d_t>>
template <typename AnalysisDomain>
class AnalysisPrinter{
private:
  Results Results;
public:
  virtual ~AnalysisPrinter() = default;
  AnalysisPrinter(struct Results Res);
  void getAnalysisResults(struct Results Res);
  virtual void emitAnalysisResults();
};

} // namespace psr
