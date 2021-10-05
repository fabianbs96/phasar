#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/ExtendedTaintAnalysis/XTaintAnalysisBase.h"
#include "phasar/PhasarLLVM/TaintConfig/TaintConfig.h"

namespace psr::XTaint {
AnalysisBase::AnalysisBase(const TaintConfig *TSF) noexcept : TSF(TSF) {
  assert(TSF != nullptr);
}

auto AnalysisBase::getConfigurationAt(const llvm::Instruction *Inst,
                                      const llvm::Instruction *Succ,
                                      const llvm::Function *Callee) const
    -> std::pair<SourceConfigTy, SinkConfigTy> {
  return {getSourceConfigAt(Inst, Succ, Callee),
          getSinkConfigAt(Inst, Succ, Callee)};
}

auto AnalysisBase::getSourceConfigAt(const llvm::Instruction *Inst,
                                     const llvm::Instruction *Succ,
                                     const llvm::Function *Callee) const
    -> SourceConfigTy {
  SourceConfigTy Ret;

  TSF->forAllGeneratedValuesAt(Inst, Succ, Callee,
                               [&Ret](const llvm::Value *V) { Ret.insert(V); });

  return Ret;
}

auto AnalysisBase::getSinkConfigAt(const llvm::Instruction *Inst,
                                   const llvm::Instruction *Succ,
                                   const llvm::Function *Callee) const
    -> SinkConfigTy {
  SinkConfigTy Ret;

  TSF->forAllLeakCandidatesAt(Inst, Succ, Callee,
                              [&Ret](const llvm::Value *V) { Ret.insert(V); });

  return Ret;
}

auto AnalysisBase::getSanitizerConfigAt(const llvm::Instruction *Inst,
                                        const llvm::Instruction *Succ,
                                        const llvm::Function *Callee) const
    -> SanitizerConfigTy {
  SanitizerConfigTy Ret;

  TSF->forAllSanitizedValuesAt(Inst, Succ, Callee,
                               [&Ret](const llvm::Value *V) { Ret.insert(V); });

  return Ret;
}
} // namespace psr::XTaint