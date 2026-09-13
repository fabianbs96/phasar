#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMBasedICFGView.h"

#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseCFGCache.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"

#include <cassert>

using namespace psr;

namespace psr {
template class LLVMBasedICFGViewMixin<SparseLLVMBasedICFGView>;
} // namespace psr

SparseLLVMBasedICFGView::SparseLLVMBasedICFGView(const LLVMBasedICFG *ICF,
                                                 LLVMAliasInfoRef PT)
    : LLVMBasedICFGViewMixin(ICF), SparseCFGCache(new SVFGCache{}),
      AliasAnalysis(PT) {}

SparseLLVMBasedICFGView::~SparseLLVMBasedICFGView() = default;

const SparseLLVMBasedCFG &
SparseLLVMBasedICFGView::getSparseCFGImpl(const llvm::Function *Fun,
                                          const llvm::Value *Val) const {
  assert(SparseCFGCache != nullptr);
  return SparseCFGCache->getOrCreate(*this, Fun, Val, AliasAnalysis);
}

auto SparseLLVMBasedICFGView::advanceToNextUserImpl(n_t Succ, v_t Fact) const
    -> n_t {
  assert(SparseCFGCache != nullptr);
  return SparseCFGCache->advanceToNextUser(Succ, Fact, AliasAnalysis);
}
