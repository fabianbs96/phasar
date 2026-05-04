#include "SVFGCache.h"

#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMBasedCFG.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"

#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Casting.h"

#include "SVFG/SVFGAliasUtils.h"

using namespace psr;
using namespace psr::svfg_detail;

static bool isFirstInBB(const llvm::Instruction *Inst) {
  return !Inst->getPrevNode();
}

static bool isLastInBB(const llvm::Instruction *Inst, const llvm::Value *Val) {
  if (Inst->getNextNode()) {
    return false;
  }

  if (Val->getType()->isPointerTy()) {
    return true;
  }

  const auto *InstBB = Inst->getParent();
  for (const auto *User : Val->users()) {
    const auto *UserInst = llvm::dyn_cast<llvm::Instruction>(User);
    if (!UserInst || UserInst->getParent() != InstBB) {
      return true;
    }
  }
  return llvm::succ_empty(Inst);
}

static bool shouldKeepInst(const llvm::Instruction *Inst,
                           const llvm::Value *Val,
                           LLVMAliasInfoRef AliasAnalysis) {
  if (Inst == Val || isFirstInBB(Inst) || isLastInBB(Inst, Val)) {
    // First in BB always stays for now
    return true;
  }

  const auto *ValTy = Val->getType();
  bool ValPtr = ValTy->isPointerTy();

  if (llvm::isa<llvm::CallBase>(Inst)) {
    if (llvm::isa<llvm::GlobalValue>(Val)) {
      return true;
    }
  }

  for (const auto *Op : Inst->operand_values()) {
    if (Op == Val) {
      return true;
    }
    if (!ValPtr) {
      continue;
    }
    const auto *OpTy = Op->getType();
    bool OpPtr = OpTy->isPointerTy();

    if (!OpPtr) {
      // Pointers cannot influence non-pointers
      continue;
    }

    if (mayAlias(Val, Op, AliasAnalysis)) {
      return true;
    }
  }

  return false;
}

static void buildSparseCFG(const LLVMBasedCFG &CFG,
                           SparseLLVMBasedCFG::vgraph_t &SCFG,
                           const llvm::Function *Fun, const llvm::Value *Val,
                           LLVMAliasInfoRef AliasAnalysis) {
  llvm::SmallVector<
      std::pair<const llvm::Instruction *, const llvm::Instruction *>>
      WL;

  // -- Initialization

  const auto *Entry = &Fun->getEntryBlock().front();
  if (llvm::isa<llvm::DbgInfoIntrinsic>(Entry)) {
    Entry = Entry->getNextNonDebugInstruction();
  }

  for (const auto *Succ : CFG.getSuccsOf(Entry)) {
    WL.emplace_back(Entry, Succ);
  }

  // -- Fixpoint Iteration

  llvm::SmallDenseSet<const llvm::Instruction *> Handled;

  while (!WL.empty()) {
    auto [From, To] = WL.pop_back_val();

    const auto *Curr = From;
    if (shouldKeepInst(To, Val, AliasAnalysis)) {
      Curr = To;
      auto [It, Inserted] = SCFG.try_emplace(From, To);
      if (!Inserted) {
        if (It->second != To) {
          It->second = nullptr;
        }
      }
    }

    if (!Handled.insert(To).second) {
      continue;
    }

    for (const auto *Succ : CFG.getSuccsOf(To)) {
      WL.emplace_back(Curr, Succ);
    }
  }
}

const SparseLLVMBasedCFG &
SVFGCache::getOrCreate(const LLVMBasedCFG &CFG, const llvm::Function *Fun,
                       const llvm::Value *Val, LLVMAliasInfoRef AliasAnalysis) {
  // XXX: Make thread-safe

  auto [It, Inserted] = Cache.try_emplace(std::make_pair(Fun, Val));
  if (Inserted) {
    buildSparseCFG(CFG, It->second.VGraph, Fun, Val, AliasAnalysis);
  }

  return It->second;
}
