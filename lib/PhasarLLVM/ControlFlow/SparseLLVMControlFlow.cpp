#include "phasar/PhasarLLVM/ControlFlow/SparseLLVMControlFlow.h"

#include "phasar/PhasarLLVM/ControlFlow/SparseControlFlowHelpers.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"

using namespace psr;

static bool mayAlias(const llvm::Value *Ptr1, const llvm::Value *Ptr2,
                     LLVMAliasInfoRef AliasAnalysis) {
  if (detail::isNonAddressTakenVariable(Ptr1) ||
      detail::isNonAddressTakenVariable(Ptr2)) {
    return false;
  }

  return AliasAnalysis.alias(Ptr1, Ptr2) != AliasResult::NoAlias;
}

bool SparseLLVMControlFlow::shouldKeepInst(n_t Inst, v_t Val,
                                           LLVMAliasInfoRef AI) {
  if (Inst == Val || detail::isSparseExitInst(Inst) || isStartInst(Inst)) {
    // First in BB always stays for now
    return true;
  }

  if (detail::isSparseNoopIntrinsic(Inst)) {
    return false;
  }

  if (llvm::isa<llvm::CallBase>(Inst)) {
    if (llvm::isa<llvm::GlobalValue>(Val)) {
      // We cannot know, whether the callee uses the global
      return true;
    }
  }

  const auto *ValTy = Val->getType();
  bool ValPtr = ValTy->isPointerTy();

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

    if (mayAlias(Val, Op, AI)) {
      return true;
    }
  }

  return false;
}

auto psr::SparseLLVMControlFlow::advanceToNextUserImplInternal(
    n_t Succ, v_t Fact, LLVMAliasInfoRef AI) -> n_t {
  const auto *Save = Succ;
  while (!shouldKeepInst(Succ, Fact, AI)) {
    n_t NextSucc =
#if LLVM_VERSION_MAJOR <= 18
        Succ->getNextNonDebugInstruction();
#else
        Succ->getNextNode();
#endif
    if (!NextSucc) {
      const auto *Parent = Succ->getParent();
      if (llvm::succ_size(Parent) == 1) {
        const auto *SuccBB = *llvm::succ_begin(Parent);
        Succ = &SuccBB->front();
#if LLVM_VERSION_MAJOR <= 18
        if (llvm::isa<llvm::DbgInfoIntrinsic>(Succ)) {
          Succ = Succ->getNextNonDebugInstruction();
        }
#endif

        if (Succ != Save && llvm::pred_size(SuccBB) == 1) {
          // just a simple chain, no merge point.
          continue;
        }

        // merge-point
        return Succ;
      }

      break;
    }
    Succ = NextSucc;
  }
  return Succ;
}
