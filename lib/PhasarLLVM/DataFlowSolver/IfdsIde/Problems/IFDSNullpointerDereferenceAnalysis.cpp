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
            llvm::outs() << *Curr << " case store \n";
          } 
          else if (const auto *Alloca = llvm::dyn_cast<llvm::AllocaInst>(Curr)) {
            llvm::outs() << *Curr <<" case alloca \n";
          }
          else if (const auto *Load = llvm::dyn_cast<llvm::LoadInst>(Curr)) {
            llvm::outs() << *Curr <<" case load \n";
      } 
      struct NDFF : FlowFunction<IFDSNullpointerDereference::d_t> {
        const llvm::StoreInst *Store;
        const llvm::Value *Zero;
        std::map<IFDSNullpointerDereference::n_t, std::set<IFDSNullpointerDereference::d_t>> &NullptrDereferences;
        NDFF(const llvm::StoreInst *S,
              std::map<IFDSNullpointerDereference::n_t, std::set<IFDSNullpointerDereference::d_t>> &NPD,
              const llvm::Value *Zero) 
              : Store(S), Zero(Zero), NullptrDereferences(NPD) {}
        std::set<IFDSNullpointerDereference::d_t>
        computeTargets(IFDSNullpointerDereference::d_t Source) override {
          if (Source == Store->getPointerOperand()) {
                return {Source, Store->getPointerOperand()};
          } else {
            return {};  // if these conditions aren't met, it isn't a nullpointer dereference
          }
        }
      };

      return std::make_shared<NDFF>(Curr, NullptrReferencesFound, getZeroValue());
}

IFDSNullpointerDereference::FlowFunctionPtrType
IFDSNullpointerDereference::getCallFlowFunction(
    IFDSNullpointerDereference::n_t CallSite,
    IFDSNullpointerDereference::f_t DestFun) {
        if (llvm::isa<llvm::CallInst>(CallSite) ||
      llvm::isa<llvm::InvokeInst>(CallSite)) {
    const auto *CS = llvm::cast<llvm::CallBase>(CallSite);
    struct NDFF : FlowFunction<IFDSNullpointerDereference::d_t> {
      const llvm::Function *DestFun;
      const llvm::CallBase *CallSite;
      const llvm::Value *Zerovalue;
      std::vector<const llvm::Value *> Actuals;
      std::vector<const llvm::Value *> Formals;
      NDFF(const llvm::Function *DM, const llvm::CallBase *CallSite,
           const llvm::Value *ZV)
          : DestFun(DM), CallSite(CallSite), Zerovalue(ZV) {
        // set up the actual parameters
        for (unsigned Idx = 0; Idx < CallSite->arg_size(); ++Idx) {
          Actuals.push_back(CallSite->getArgOperand(Idx));
        }

        for (const auto &Arg : DestFun->args()) {
          Formals.push_back(&Arg);
        }
      }

      std::set<IFDSNullpointerDereference::d_t>
      computeTargets(IFDSNullpointerDereference::d_t Source) override {
        // perform parameter passing
        if (Source != Zerovalue) {
          std::set<const llvm::Value *> Res;
          for (unsigned Idx = 0; Idx < Formals.size(); ++Idx) {
            if (Source == Actuals[Idx]) {
              Res.insert(Formals[Idx]);
            }
          }
          return Res;
        }

        // propagate zero
        return {Source};
      }
    };
    return std::make_shared<NDFF>(DestFun, CS, getZeroValue());
  }
  return Identity<IFDSNullpointerDereference::d_t>::getInstance();
}

IFDSNullpointerDereference::FlowFunctionPtrType
IFDSNullpointerDereference::getRetFlowFunction(
    IFDSNullpointerDereference::n_t CallSite,
    IFDSNullpointerDereference::f_t /*CalleeFun*/,
    IFDSNullpointerDereference::n_t ExitStmt,
    IFDSNullpointerDereference::n_t /*RetSite*/) {
  if (llvm::isa<llvm::CallInst>(CallSite) ||
      llvm::isa<llvm::InvokeInst>(CallSite)) {
    const auto *CS = llvm::cast<llvm::CallBase>(CallSite);
    struct NDFF : FlowFunction<IFDSNullpointerDereference::d_t> {
      const llvm::CallBase *Call;
      const llvm::Instruction *Exit;
      NDFF(const llvm::CallBase *C, const llvm::Instruction *E)
          : Call(C), Exit(E) {}
      std::set<IFDSNullpointerDereference::d_t>
      computeTargets(IFDSNullpointerDereference::d_t Source) override {
        // check if we return a nullpointer
        std::set<IFDSNullpointerDereference::d_t> Ret;
        //----------------------------------------------------------------------
        // Handle pointer/reference parameters
        //----------------------------------------------------------------------
        if (Call->getCalledFunction()) {
          unsigned I = 0;
          for (const auto &Arg : Call->getCalledFunction()->args()) {
            // auto arg = getNthFunctionArgument(call.getCalledFunction(), i);
            if (&Arg == Source && Arg.getType()->isPointerTy()) {
              Ret.insert(Call->getArgOperand(I));
            }
            I++;
          }
        }

        // kill all other facts
        return Ret;
      }
    };
    return std::make_shared<NDFF>(CS, ExitStmt);
  }
  // kill everything else
  return killAllFlows<d_t>();
}

IFDSNullpointerDereference::FlowFunctionPtrType
IFDSNullpointerDereference::getCallToRetFlowFunction(
    IFDSNullpointerDereference::n_t CallSite,
    IFDSNullpointerDereference::n_t /*RetSite*/,
    llvm::ArrayRef<IFDSNullpointerDereference::f_t> /*Callees*/) {
  if (const auto *CS = llvm::dyn_cast<llvm::CallBase>(CallSite)) {
    return lambdaFlow<psr::IFDSNullpointerDereference::d_t>(
        [CS](psr::IFDSNullpointerDereference::d_t Source)
            -> std::set<psr::IFDSNullpointerDereference::d_t> {
          if (Source->getType()->isPointerTy()) {
            for (const auto &Arg : CS->args()) {
              if (Arg.get() == Source) {
                return {};
              }
            }
          }
          return {Source};
        });
  }
  return Identity<psr::IFDSNullpointerDereference::d_t>::getInstance();
}

InitialSeeds<IFDSNullpointerDereference::n_t, IFDSNullpointerDereference::d_t,
             IFDSNullpointerDereference::l_t>
IFDSNullpointerDereference::initialSeeds() {
  PHASAR_LOG_LEVEL(DEBUG, "IFDSNullpointerDereference::initialSeeds()");
  InitialSeeds<IFDSNullpointerDereference::n_t, IFDSNullpointerDereference::d_t,
               IFDSNullpointerDereference::l_t>
      Seeds;
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

}