#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/Pointer/LLVMPointsToInfo.h"
#include "phasar/PhasarLLVM/Pointer/LLVMRawAAResults.h"
#include "phasar/Pointer/RawPointsToResult.h"
#include "phasar/Utils/MaybeUniquePtr.h"

#include "llvm/IR/Argument.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"

namespace psr {

/// True if \p V can name an abstract object: a stack or heap allocation, a
/// global, a function, or a parameter of an entry point.
[[nodiscard]] inline bool isAbstractObject(const llvm::Value *V) noexcept {
  return llvm::isa<llvm::AllocaInst, llvm::CallBase, llvm::Argument,
                   llvm::Constant>(V);
}

/// Returns a \c ValueId handler suitable for \c RawAliasSet::foreach() that
/// reports the allocation-site names of each abstract object.
///
/// A node can carry further names -- a load whose result the analysis knows
/// to be the object, a GEP on its base, ... -- that do not denote objects and
/// must not show up in a points-to set.
constexpr std::invocable<ValueId> auto
llvmRawPointeeHandler(const ValueCompressor<PAGVariable> &VC,
                      std::invocable<const llvm::Value *> auto Callback) {
  return [&VC, Callback{copyOrRef(Callback)}](ValueId Obj) {
    for (auto V : VC.id2vars(Obj)) {
      if (const auto *LLVMVar = V.valueOrNull();
          LLVMVar && isAbstractObject(LLVMVar)) {
        std::invoke(Callback, LLVMVar);
      }
    }
  };
}

/// Adds the LLVM points-to-iterator interface to a \c RawPointsToResult,
/// reporting pointees as \c llvm::Value* via the stored \c ValueCompressor.
///
/// Satisfies \c IsPointsToIterator.
///
/// \tparam PTResT A type satisfying \c RawPointsToResult. May be a reference
/// type to obtain a non-owning view on an existing result.
template <typename PTResT>
  requires RawPointsToResult<std::remove_cvref_t<PTResT>>
struct LLVMRawPointsToIterator {
  [[no_unique_address]] PTResT PTRes;
  MaybeUniquePtr<const ValueCompressor<PAGVariable>> VC;

  using v_t = const llvm::Value *;
  using o_t = const llvm::Value *;
  using n_t = const llvm::Instruction *;

  [[nodiscard]] const auto &base() const noexcept { return PTRes; }

  [[nodiscard]] static constexpr o_t asAbstractObject(v_t Pointer) noexcept {
    return Pointer;
  }

  [[nodiscard]] decltype(auto) getRawPointsToSet(ValueId Ptr) const {
    return PTRes.getRawPointsToSet(Ptr);
  }

  void
  forallPointeesOf(ValueId Ptr, const auto & /*At*/,
                   std::invocable<const llvm::Value *> auto WithPointee) const {
    PTRes.getRawPointsToSet(Ptr).foreach (
        llvmRawPointeeHandler(*VC, copyOrRef(WithPointee)));
  }

  void
  forallPointeesOf(const llvm::Value *Ptr, const auto &At,
                   std::invocable<const llvm::Value *> auto WithPointee) const {
    if (auto PtrId = VC->getOrNull(Ptr)) {
      forallPointeesOf(*PtrId, At, copyOrRef(WithPointee));
    }
  }

  [[nodiscard]] bool mayPointsTo(ValueId Ptr, ValueId Obj) const {
    return PTRes.mayPointsTo(Ptr, Obj);
  }

  [[nodiscard]] bool mayPointsTo(ValueId Ptr, ValueId Obj,
                                 const auto & /*AtInstruction*/) const {
    return PTRes.mayPointsTo(Ptr, Obj);
  }

  [[nodiscard]] bool mayPointsTo(const llvm::Value *Ptr,
                                 const llvm::Value *Obj) const {
    if (!isAbstractObject(Obj)) {
      return false;
    }

    auto PtrId = VC->getOrNull(Ptr);
    auto ObjId = VC->getOrNull(Obj);

    return PtrId && ObjId && PTRes.mayPointsTo(*PtrId, *ObjId);
  }

  [[nodiscard]] bool mayPointsTo(const llvm::Value *Ptr, const llvm::Value *Obj,
                                 const auto & /*AtInstruction*/) const {
    return mayPointsTo(Ptr, Obj);
  }
};

} // namespace psr
