#include "phasar/PhasarLLVM/DataFlow/IfdsIde/LLVMFunctionDataFlowFacts.h"

#include "phasar/DataFlow/IfdsIde/Solver/IFDSSolver.h"
#include "phasar/PhasarLLVM/Domain/LLVMAnalysisDomain.h"
#include "phasar/Utils/Table.h"

#include "llvm/IR/Instructions.h"

#include <llvm-14/llvm/IR/Instruction.h>

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

LLVMFunctionDataFlowFacts
psr::library_summary::LLVMFunctionDataFlowFacts::convertFromEndsummaryTab(
    const Table<llvm::Instruction *, llvm::Value *,
                Table<llvm::Instruction *, llvm::Value *,
                      EdgeFunction<BinaryDomain>>> &EST) {
  LLVMFunctionDataFlowFacts FromEndsumTab;
  EST.foreachCell([&FromEndsumTab](const llvm::Instruction *RowKey,
                                   const llvm::Value *ColumnKey,
                                   const auto &Value) {
    if (auto const &FactIn = llvm::dyn_cast<llvm::Argument>(ColumnKey)) {
      const llvm::Function *Fun = FactIn->getParent();
      Value.foreachCell(
          [Fun, &FactIn, &FromEndsumTab](const llvm::Instruction *InnerRowKey,
                                         const llvm::Value *InnerColumnKey,
                                         const auto & /*InnerValue*/) {
            if (auto const &FactOut =
                    llvm::dyn_cast<llvm::Argument>(InnerColumnKey)) {
              FromEndsumTab.addElement(
                  Fun, FactIn->getArgNo(),
                  Parameter{static_cast<uint16_t>(FactOut->getArgNo())});
            } else {
              for (const auto &BBIterator : *Fun) {
                if (auto const &RetInst = llvm::dyn_cast<llvm::ReturnInst>(
                        BBIterator->getTerminator())) {
                  if (FactOut == RetInst.ReturnValue()) {
                    FromEndsumTab.addElement(Fun, FactIn->getArgNo(),
                                             ReturnValue{});
                  }
                }
              }
            }
          });
    }
  });
  return FromEndsumTab;
}
