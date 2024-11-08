#include "phasar/PhasarLLVM/DataFlow/IfdsIde/LLVMFunctionDataFlowFacts.h"

#include "phasar/DataFlow/IfdsIde/EdgeFunction.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Domain/LLVMAnalysisDomain.h"
#include "phasar/Utils/Table.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

using namespace psr;
using namespace psr::library_summary;

LLVMFunctionDataFlowFacts
library_summary::readFromFDFF(const FunctionDataFlowFacts &Fdff,
                              const LLVMProjectIRDB &Irdb) {
  LLVMFunctionDataFlowFacts Llvmfdff;
  Llvmfdff.LLVMFdff.reserve(Fdff.size());

  for (const auto &It : Fdff) {
    if (const llvm::Function *Fun = Irdb.getFunction(It.first())) {
      Llvmfdff.LLVMFdff.try_emplace(Fun, It.second);
    }
  }
  return Llvmfdff;
}

LLVMFunctionDataFlowFacts LLVMFunctionDataFlowFacts::fromEndsummaryTab(
    const DefaultIFDSEndSummaryTabTy &EST) {
  LLVMFunctionDataFlowFacts FromEndsumTab;
  EST.foreachCell([&FromEndsumTab](const llvm::Instruction * /*RowKey*/,
                                   const llvm::Value *ColumnKey,
                                   const auto &Value) {
    const auto *FactIn = llvm::dyn_cast<llvm::Argument>(ColumnKey);
    if (!FactIn) {
      // For now, only care about path-edges that start with an argument.

      // XXX: Later, care about zero as well
      return;
    }
    const llvm::Function *Fun = FactIn->getParent();
    Value.foreachCell([Fun, FactIn, &FromEndsumTab](
                          const llvm::Instruction * /*InnerRowKey*/,
                          const llvm::Value *InnerColumnKey,
                          const auto & /*InnerValue*/) {
      if (auto const &FactOut =
              llvm::dyn_cast<llvm::Argument>(InnerColumnKey)) {
        FromEndsumTab.addElement(
            Fun, FactIn->getArgNo(),
            Parameter{static_cast<uint16_t>(FactOut->getArgNo())});
        return;
      }
      if (Fun->getReturnType()->isVoidTy()) {
        return;
      }
      for (const auto &BB : *Fun) {
        if (auto const *RetInst =
                llvm::dyn_cast<llvm::ReturnInst>(BB.getTerminator())) {
          if (InnerColumnKey == RetInst->getReturnValue()) {
            FromEndsumTab.addElement(Fun, FactIn->getArgNo(), ReturnValue{});
          }
        }
      }
    });
  });
  return FromEndsumTab;
}
