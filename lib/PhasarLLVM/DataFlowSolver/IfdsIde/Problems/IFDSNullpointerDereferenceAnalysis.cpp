/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/IFDSNullpointerDereferenceAnalysis.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/FlowFunctions.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/LLVMFlowFunctions.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/IFDSIDESolverConfig.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/LLVMZeroValue.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Logger.h"
#include "phasar/DB/LLVMProjectIRDB.h"

#include "llvm/IR/AbstractCallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <set>
#include <llvm/Support/Casting.h>

namespace psr {

IFDSNullpointerDereference::IFDSNullpointerDereference(
    const LLVMProjectIRDB *IRDB, std::vector<std::string> EntryPoints)
    : IFDSTabulationProblem(IRDB, std::move(EntryPoints), LLVMZeroValue::getInstance()) {
}

IFDSNullpointerDereference::FlowFunctionPtrType
IFDSNullpointerDereference::getNormalFlowFunction(
    IFDSNullpointerDereference::n_t Curr,
    IFDSNullpointerDereference::n_t /*Succ*/) {
      if (const auto *Store = llvm::dyn_cast<llvm::StoreInst>(Curr)) {
        // both store cases
        // nested if's should be avoided, however, this is a quick and (hopefully) working version
        if (llvm::isa<llvm::ConstantPointerNull>(Store->getValueOperand()))  {
          llvm::outs() << *Curr << " case store with nullptr \n";
          return generateFlow(Store->getPointerOperand(), getZeroValue());
        }

        llvm::outs() << *Curr << " case store not nullptr \n";
        return strongUpdateStore(Store);
      } 
      
      // alloca case
      if (const auto *Alloca = llvm::dyn_cast<llvm::AllocaInst>(Curr)) {
        llvm::outs() << *Curr <<" case alloca \n";
        return generateFlow(Alloca, getZeroValue());
      }

      // load case
      if (const auto *Load = llvm::dyn_cast<llvm::LoadInst>(Curr)) {
        llvm::outs() << *Curr <<" case load \n";
        return generateFlow(Load, Load->getPointerOperand());
      } 

      // test code so that this file compiles. Needs a return 
      return Identity<IFDSNullpointerDereference::d_t>::getInstance();
}

IFDSNullpointerDereference::FlowFunctionPtrType
IFDSNullpointerDereference::getCallFlowFunction(
    IFDSNullpointerDereference::n_t CallSite,
    IFDSNullpointerDereference::f_t DestFun) {
        // propagate all
      if (const auto *CS = llvm::dyn_cast<llvm::CallBase>(CallSite)) 
      {
        return Identity<IFDSNullpointerDereference::d_t>::getInstance();
      }

      return killAllFlows<d_t>();
}

IFDSNullpointerDereference::FlowFunctionPtrType
IFDSNullpointerDereference::getRetFlowFunction(
    IFDSNullpointerDereference::n_t CallSite,
    IFDSNullpointerDereference::f_t /*CalleeFun*/,
    IFDSNullpointerDereference::n_t ExitStmt,
    IFDSNullpointerDereference::n_t /*RetSite*/) {
      return mapFactsToCaller(
        llvm::cast<llvm::CallBase>(CallSite), ExitStmt,
        [](d_t Formal, d_t Source) {
          return Formal == Source && Formal->getType()->isPointerTy();
        },
        [](d_t RetVal, d_t Source) { return RetVal == Source; });

      // kill everything else
      return killAllFlows<d_t>();
}

IFDSNullpointerDereference::FlowFunctionPtrType
IFDSNullpointerDereference::getCallToRetFlowFunction(
    IFDSNullpointerDereference::n_t CallSite,
    IFDSNullpointerDereference::n_t /*RetSite*/,
    llvm::ArrayRef<IFDSNullpointerDereference::f_t> /*Callees*/) { 
      if (const auto *CS = llvm::dyn_cast<llvm::CallBase>(CallSite)) {
        return lambdaFlow<IFDSNullpointerDereference::d_t>(
            [CS](IFDSNullpointerDereference::d_t Source)
                 -> std::set<IFDSNullpointerDereference::d_t> {
              // everything but pointer type variables must be propagated
              if (!Source->getType()->isPointerTy()) { 
                return {Source};
              }
              return {};
        });
      }
      // kill everything else
      return killAllFlows<d_t>();
}

InitialSeeds<IFDSNullpointerDereference::n_t, 
    IFDSNullpointerDereference::d_t,
    IFDSNullpointerDereference::l_t>
    IFDSNullpointerDereference::initialSeeds() {
      PHASAR_LOG_LEVEL(DEBUG, "IFDSNullpointerDereference::initialSeeds()");
      InitialSeeds<IFDSNullpointerDereference::n_t, 
      IFDSNullpointerDereference::d_t, 
      IFDSNullpointerDereference::l_t> Seeds;

      for (const auto &EntryPoint : EntryPoints) {
        Seeds.addSeed(&IRDB->getFunction(EntryPoint)->front().front(),
                      getZeroValue());
      }

      return Seeds;
}

bool IFDSNullpointerDereference::isZeroValue(
    IFDSNullpointerDereference::d_t Fact) const {
      return LLVMZeroValue::getInstance()->isLLVMZeroValue(Fact);
}

void IFDSNullpointerDereference::printNode(llvm::raw_ostream &OS,
    IFDSNullpointerDereference::n_t Inst) const {
      OS << llvmIRToString(Inst);
}

void IFDSNullpointerDereference::printDataFlowFact(
    llvm::raw_ostream &Os, IFDSNullpointerDereference::d_t FlowFact) const {
     Os << llvmIRToString(FlowFact);
}

void IFDSNullpointerDereference::printFunction(llvm::raw_ostream &Os,
    IFDSNullpointerDereference::f_t Fun) const {
      Os << Fun->getName();
}

} // namespace psr