#include "phasar/PhasarLLVM/DataFlow/IfdsIde/LLVMFunctionDataFlowFacts.h"

#include "phasar/DataFlow/IfdsIde/Solver/IFDSSolver.h"
#include "phasar/PhasarLLVM/Domain/LLVMAnalysisDomain.h"

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

LLVMFunctionDataFlowFacts
psr::library_summary::LLVMFunctionDataFlowFacts::convertFromEndsummaryTab(
    IFDSSolver<LLVMIFDSAnalysisDomainDefault> &Solver) {
  auto const &SolverEST = Solver.getEndsummaryTab();
  LLVMFunctionDataFlowFacts FromEndsumTab;
  SolverEST.foreachCell([&FromEndsumTab](const llvm::Instruction *RowKey,
                                         const llvm::Value *ColumnKey,
                                         decltype((SolverEST.get(
                                             RowKey, ColumnKey))) Value) {
    if (auto const &FactIn = llvm::dyn_cast<llvm::Argument>(ColumnKey)) {
      const llvm::Function *FlowFunc = FactIn->getParent();
      Value.foreachCell([FlowFunc, &FactIn, &FromEndsumTab](
                            const llvm::Instruction *InnerRowKey,
                            const llvm::Value *InnerColumnKey,
                            decltype((Value.get(
                                InnerRowKey, InnerColumnKey))) /*InnerValue*/) {
        if (auto const &FactOut =
                llvm::dyn_cast<llvm::Argument>(InnerColumnKey)) {
          FromEndsumTab.addElement(
              FlowFunc, FactIn->getArgNo(),
              Parameter{static_cast<uint16_t>(FactOut->getArgNo())});
        } else {
          // TODO

          if (auto const &Inst = FlowFunc->begin()->getTerminator()) {
            if (auto const &RetInst = llvm::dyn_cast<llvm::ReturnInst>(Inst)) {
              FromEndsumTab.addElement(FlowFunc, FactIn->getArgNo(),
                                       ReturnValue{});
            }
          }
        }
      });
    }
  });
  return FromEndsumTab;
}
