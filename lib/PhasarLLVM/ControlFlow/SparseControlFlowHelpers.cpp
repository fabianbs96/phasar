#include "phasar/PhasarLLVM/ControlFlow/SparseControlFlowHelpers.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Casting.h"

#if LLVM_VERSION_MAJOR <= 20
static bool isNonPointerType(const llvm::Type *Ty) {
  if (const auto *Struct = llvm::dyn_cast<llvm::StructType>(Ty)) {
    for (const auto *ElemTy : Struct->elements()) {
      // XXX: Go into nested structs recursively
      if (!ElemTy->isSingleValueType() || ElemTy->isVectorTy()) {
        return false;
      }
    }
    return true;
  }
  if (const auto *Vec = llvm::dyn_cast<llvm::VectorType>(Ty)) {
    return !Vec->getElementType()->isPointerTy();
  }
  return Ty->isSingleValueType();
}
#endif

bool psr::detail::isSparseNoopIntrinsic(const llvm::Instruction *Inst) {
  // isAssumeLikeIntrinsic() alone is not enough: llvm.objectsize and
  // llvm.ptr_annotation derive their result from a pointer operand
  const auto *II = llvm::dyn_cast<llvm::IntrinsicInst>(Inst);
  return II && II->isAssumeLikeIntrinsic() && II->getType()->isVoidTy();
}

bool psr::detail::isSparseExitInst(const llvm::Instruction *Inst) {
  return llvm::isa<llvm::ReturnInst, llvm::ResumeInst, llvm::UnreachableInst>(
      Inst);
}

bool psr::detail::isNonAddressTakenVariable(const llvm::Value *Val) {
  const auto *Alloca = llvm::dyn_cast<llvm::AllocaInst>(Val);
  if (!Alloca) {
    return false;
  }
  for (const auto &Use : Alloca->uses()) {
    if (const auto *Store = llvm::dyn_cast<llvm::StoreInst>(Use.getUser())) {
      if (Use == Store->getValueOperand()) {
        return false;
      }
    } else if (const auto *Call =
                   llvm::dyn_cast<llvm::CallBase>(Use.getUser())) {
      auto ArgNo = Use.getOperandNo();
      if (Call->paramHasAttr(ArgNo, llvm::Attribute::StructRet)) {
        continue;
      }
#if LLVM_VERSION_MAJOR <= 20
      if (Call->paramHasAttr(ArgNo, llvm::Attribute::NoCapture) &&
          isNonPointerType(Call->getType())) {
        continue;
      }
      return false;
#else
      auto Captures = Call->getCaptureInfo(ArgNo);
      auto CComp = Captures.getOtherComponents() | Captures.getRetComponents();
      if (llvm::capturesAnyProvenance(CComp) ||
          (llvm::capturesAddress(CComp) &&
           !llvm::capturesAddressIsNullOnly(CComp))) {
        return false;
      }
      continue;
#endif
    }
  }
  return true;
}
