#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/ExtendedTaintAnalysis/XTaintEdgeFunctionCache.h"
#include "phasar/PhasarLLVM/Utils/BasicBlockOrdering.h"
#include <tuple>
#include <utility>

namespace psr::XTaint {
static inline llvm::DenseMap<
    std::pair<BasicBlockOrdering *, const llvm::Instruction *>,
    std::weak_ptr<EdgeFunction<EdgeDomain>>>
    GenCache;
static inline llvm::DenseMap<
    std::tuple<BasicBlockOrdering *, const EdgeFunction<EdgeDomain> *,
               const EdgeFunction<EdgeDomain> *>,
    std::weak_ptr<EdgeFunction<EdgeDomain>>>
    ComposeCache;

std::weak_ptr<EdgeFunction<EdgeDomain>> &
EdgeFunctionCache::GenEdgeFunction(BasicBlockOrdering &BBO,
                                   const llvm::Instruction *Inst) {
  return GenCache[std::make_pair(&BBO, Inst)];
}

std::weak_ptr<EdgeFunction<EdgeDomain>> &
EdgeFunctionCache::ComposeEdgeFunction(BasicBlockOrdering &BBO,
                                       const EdgeFunction<EdgeDomain> *F,
                                       const EdgeFunction<EdgeDomain> *G) {
  return ComposeCache[std::make_tuple(&BBO, F, G)];
}

void EdgeFunctionCache::clear() {
  GenCache.clear();
  ComposeCache.clear();
}
} // namespace psr::XTaint