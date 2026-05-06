#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

// Internal header: shared alias helpers used by SVFGCache and SVFGBuilder.

#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"

namespace psr::svfg_detail {

[[nodiscard]] inline bool isNonPointerType(const llvm::Type *Ty) noexcept {
  if (const auto *Struct = llvm::dyn_cast<llvm::StructType>(Ty)) {
    for (const auto *ElemTy : Struct->elements()) {
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

[[nodiscard]] inline bool
isNonAddressTakenVariable(const llvm::Value *Val) noexcept {
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
      if (Call->paramHasAttr(ArgNo, llvm::Attribute::NoCapture) &&
          isNonPointerType(Call->getType())) {
        continue;
      }
      return false;
    }
  }
  return true;
}

[[nodiscard]] inline bool mayAlias(const llvm::Value *Ptr1,
                                   const llvm::Value *Ptr2,
                                   LLVMAliasInfoRef AA) noexcept {
  if (Ptr1 == Ptr2) {
    return true;
  }
  if (isNonAddressTakenVariable(Ptr1) || isNonAddressTakenVariable(Ptr2)) {
    return false;
  }
  return AA.alias(Ptr1, Ptr2) != AliasResult::NoAlias;
}

} // namespace psr::svfg_detail
