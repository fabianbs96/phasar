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
                                         const auto &Value) {
    if (auto const &FactIn = llvm::dyn_cast<llvm::Argument>(ColumnKey)) {
      const llvm::Function *FlowFunc = FactIn->getParent();
      Value.foreachCell([FlowFunc, &FactIn,
                         &FromEndsumTab](const llvm::Instruction *InnerRowKey,
                                         const llvm::Value *InnerColumnKey,
                                         const auto & /*InnerValue*/) {
        if (auto const &FactOut =
                llvm::dyn_cast<llvm::Argument>(InnerColumnKey)) {
          FromEndsumTab.addElement(
              FlowFunc, FactIn->getArgNo(),
              Parameter{static_cast<uint16_t>(FactOut->getArgNo())});
        } else {
          const auto BBIterator = FlowFunc->begin();
          while (BBIterator != FlowFunc->end()) {
            // range based for loop to iterate over basicblocks
            // for (const auto BBIterator : FlowFunc->getBasicBlockList()) {
            // -> no public method to retrieve BasicBlocks
            if (auto const &RetInst =
                    llvm::dyn_cast<llvm::ReturnInst>(BBIterator)) {
              if (FactOut->getType() == RetInst.getType()) {
                FromEndsumTab.addElement(FlowFunc, FactIn->getArgNo(),
                                         ReturnValue{});
              }
            }
            BBIterator++;
          }
          /*if (auto const &Inst = FlowFunc->begin()->getTerminator()) {
            if (auto const &RetInst = llvm::dyn_cast<llvm::ReturnInst>(Inst)) {
              // compare FactOut and RetInst value
              FromEndsumTab.addElement(FlowFunc, FactIn->getArgNo(),
                                       ReturnValue{});
            }
          }*/
        }
      });
    }
  });
  return FromEndsumTab;
}
