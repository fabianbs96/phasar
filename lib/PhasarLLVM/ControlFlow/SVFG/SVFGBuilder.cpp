/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFGBuilder.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"

#include "SVFGAliasUtils.h"

using namespace psr;
using namespace psr::svfg_detail;

SVFGBuilder::SVFGBuilder(const LLVMBasedICFG &ICFG,
                         LLVMAliasInfoRef AA) noexcept
    : ICFGPtr(&ICFG), AliasAnalysis(AA) {}

static std::optional<SVFGNodeKind>
classifyInstruction(const llvm::Instruction *I) noexcept {
  if (llvm::isa<llvm::AllocaInst>(I)) {
    return SVFGNodeKind::Addr;
  }
  if (llvm::isa<llvm::GetElementPtrInst>(I)) {
    return SVFGNodeKind::Gep;
  }
  if (llvm::isa<llvm::StoreInst>(I)) {
    return SVFGNodeKind::Store;
  }
  if (llvm::isa<llvm::LoadInst>(I)) {
    return SVFGNodeKind::Load;
  }
  if (const auto *Ret = llvm::dyn_cast<llvm::ReturnInst>(I)) {
    return Ret->getReturnValue() ? std::optional{SVFGNodeKind::FormalRet}
                                 : std::nullopt;
  }
  if (llvm::isa<llvm::CallBase>(I)) {
    return I->getType()->isVoidTy() ? std::nullopt
                                    : std::optional{SVFGNodeKind::ActualRet};
  }
  if (llvm::isa<llvm::PHINode>(I) || llvm::isa<llvm::SelectInst>(I) ||
      llvm::isa<llvm::CastInst>(I)) {
    return SVFGNodeKind::Copy;
  }
  // Other value-producing instructions (arithmetic, logic, etc.)
  if (!I->getType()->isVoidTy()) {
    return SVFGNodeKind::Copy;
  }
  return std::nullopt;
}

void SVFGBuilder::buildIntraSSA(const llvm::Function *F, SVFG &G) {
  llvm::DenseMap<const llvm::Value *, SVFGNodeId> LocalMap;

  // Function parameters
  for (const auto &Arg : F->args()) {
    auto Id = G.addNode({SVFGNodeKind::FormalParam, &Arg, nullptr, F});
    LocalMap[&Arg] = Id;
  }

  // Instructions
  for (const auto &BB : *F) {
    for (const auto &I : BB) {
      auto Kind = classifyInstruction(&I);
      if (!Kind) {
        continue;
      }
      auto Id = G.addNode({*Kind, &I, nullptr, F});
      LocalMap[&I] = Id;
    }
  }

  // SSA def-use edges within this function
  for (const auto &[Val, FromId] : LocalMap) {
    for (const auto *User : Val->users()) {
      const auto *UserInst = llvm::dyn_cast<llvm::Instruction>(User);
      if (!UserInst || UserInst->getFunction() != F) {
        continue;
      }
      auto It = LocalMap.find(UserInst);
      if (It != LocalMap.end()) {
        G.addEdge(FromId, It->second, SVFGEdgeKind::Direct);
      }
    }
  }
}

void SVFGBuilder::buildIndirect(const llvm::Function *F, SVFG &G) {
  llvm::SmallVector<const llvm::StoreInst *> Stores;
  llvm::SmallVector<const llvm::LoadInst *> Loads;

  for (const auto &BB : *F) {
    for (const auto &I : BB) {
      if (const auto *S = llvm::dyn_cast<llvm::StoreInst>(&I)) {
        Stores.push_back(S);
      } else if (const auto *L = llvm::dyn_cast<llvm::LoadInst>(&I)) {
        Loads.push_back(L);
      }
    }
  }

  for (const auto *S : Stores) {
    auto StoreNodes = G.nodesFor(S);
    if (StoreNodes.empty()) {
      continue;
    }
    auto StoreId = StoreNodes[0];
    for (const auto *L : Loads) {
      if (mayAlias(S->getPointerOperand(), L->getPointerOperand(),
                   AliasAnalysis)) {
        auto LoadNodes = G.nodesFor(L);
        if (!LoadNodes.empty()) {
          G.addEdge(StoreId, LoadNodes[0], SVFGEdgeKind::Indirect);
        }
      }
    }
  }
}

void SVFGBuilder::buildInterProc(const llvm::CallBase *CS,
                                 const llvm::Function *Callee, SVFG &G) {
  const auto *CallerFun = CS->getFunction();

  // Parameter binding
  unsigned ArgIdx = 0;
  for (const auto &Param : Callee->args()) {
    if (ArgIdx >= CS->arg_size()) {
      break;
    }
    const auto *ActualArg = CS->getArgOperand(ArgIdx++);

    auto APId =
        G.addNode({SVFGNodeKind::ActualParam, ActualArg, CS, CallerFun});
    auto FPNodes = G.nodesFor(&Param);
    // Connect arg definition -> ActualParam
    for (auto ArgNodeId : G.nodesFor(ActualArg)) {
      G.addEdge(ArgNodeId, APId, SVFGEdgeKind::Direct);
    }
    // Connect ActualParam -> FormalParam (Call edge)
    for (auto FPId : FPNodes) {
      G.addEdge(APId, FPId, SVFGEdgeKind::Call);
    }
  }

  // Return value binding
  if (CS->getType()->isVoidTy() || Callee->getReturnType()->isVoidTy()) {
    return;
  }
  // The ActualRet node was created for the call instruction in Phase 1
  auto ARNodes = G.nodesFor(CS);
  if (ARNodes.empty()) {
    return;
  }
  auto ARId = ARNodes[0];

  for (const auto &BB : *Callee) {
    const auto *Ret = llvm::dyn_cast<llvm::ReturnInst>(BB.getTerminator());
    if (!Ret || !Ret->getReturnValue()) {
      continue;
    }
    for (auto FRId : G.nodesFor(Ret)) {
      G.addEdge(FRId, ARId, SVFGEdgeKind::Return);
    }
  }
}

void SVFGBuilder::addFunction(const llvm::Function *F, SVFG &G) {
  if (!Processed.insert(F).second) {
    return;
  }
  buildIntraSSA(F, G);
  buildIndirect(F, G);
}

SVFG SVFGBuilder::build() {
  SVFG G;

  // Pass 1: intra-procedural nodes and edges for all reachable functions
  for (const auto *F : ICFGPtr->getAllVertexFunctions()) {
    addFunction(F, G);
  }

  // Pass 2: inter-procedural boundary edges (requires all callee nodes to
  // exist)
  for (const auto *F : ICFGPtr->getAllVertexFunctions()) {
    for (const auto *CSInst : ICFGPtr->getCallsFromWithin(F)) {
      const auto *CS = llvm::cast<llvm::CallBase>(CSInst);
      for (const auto *Callee : ICFGPtr->getCalleesOfCallAt(CSInst)) {
        buildInterProc(CS, Callee, G);
      }
    }
  }

  G.finalize();
  return G;
}
