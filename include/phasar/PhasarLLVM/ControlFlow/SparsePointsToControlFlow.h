#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/ControlFlow/SparseCFGProvider.h"
#include "phasar/PhasarLLVM/ControlFlow/SparseControlFlowHelpers.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Pointer/PointsToIterator.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Value.h"

#include <type_traits>

namespace psr {

/// \brief Points-to-based analogue of SparseLLVMControlFlow.
template <detail::HasMayPointsTo PTRef>
  requires detail::HasAsAbstractObject<PTRef>
class SparsePointsToControlFlow {
public:
  using n_t = const llvm::Instruction *;
  using v_t = const llvm::Value *;
  using o_t = typename PTRef::o_t;

  /// \brief Advances Succ to the next instruction that may use Fact.
  /// Fact may be an o_t, or any D whose valueOf(D) converts to v_t.
  template <typename D>
  [[nodiscard]] static n_t
  advanceToNextUser(n_t Succ, const D &Fact, PTRef PT,
                    const llvm::DenseSet<o_t> &KnownGlobals) {
    return advanceToNextUserImpl(Succ, objectOf(Fact, PT), PT, KnownGlobals);
  }

  /// \brief Whether Inst may use Val, based on the given points-to
  /// information.
  [[nodiscard]] static bool
  shouldKeepInst(n_t Inst, o_t Val, PTRef PT,
                 const llvm::DenseSet<o_t> &KnownGlobals);

private:
  template <typename D>
  [[nodiscard]] static o_t objectOf(const D &Fact, PTRef PT) {
    if constexpr (std::is_convertible_v<D, o_t>) {
      return Fact;
    } else {
      using psr::valueOf;
      return PT.asAbstractObject(valueOf(Fact));
    }
  }

  [[nodiscard]] static n_t
  advanceToNextUserImpl(n_t Succ, o_t Val, PTRef PT,
                        const llvm::DenseSet<o_t> &KnownGlobals) {
    if (PT.asAbstractObject(Succ) == Val || detail::isSparseExitInst(Succ) ||
        isStartInst(Succ)) {
      return Succ;
    }
    if (llvm::isa<llvm::CallBase>(Succ) &&
        !detail::isSparseNoopIntrinsic(Succ) && KnownGlobals.contains(Val)) {
      return Succ;
    }

    return advanceToNextUserImplInternal(Succ, Val, PT, KnownGlobals);
  }

  [[nodiscard]] static n_t
  advanceToNextUserImplInternal(n_t Succ, o_t Val, PTRef PT,
                                const llvm::DenseSet<o_t> &KnownGlobals);
};

template <detail::HasMayPointsTo PTRef>
  requires detail::HasAsAbstractObject<PTRef>
bool SparsePointsToControlFlow<PTRef>::shouldKeepInst(
    n_t Inst, o_t Val, PTRef PT, const llvm::DenseSet<o_t> &KnownGlobals) {
  if (PT.asAbstractObject(Inst) == Val || detail::isSparseExitInst(Inst) ||
      isStartInst(Inst)) {
    // First in BB always stays for now
    return true;
  }

  if (detail::isSparseNoopIntrinsic(Inst)) {
    return false;
  }

  if (llvm::isa<llvm::CallBase>(Inst) && KnownGlobals.contains(Val)) {
    // We cannot know, whether the callee uses the global
    return true;
  }

  // If Val itself never escapes, nothing but a direct reference to it
  // (Op == Val, checked below) can ever reach its memory.
  bool ValNonEscaping = false;
  if constexpr (std::is_convertible_v<o_t, v_t>) {
    ValNonEscaping = detail::isNonAddressTakenVariable(v_t(Val));
  }

  for (const auto *Op : Inst->operand_values()) {
    o_t OpObj = PT.asAbstractObject(Op);
    if (OpObj == Val) {
      return true;
    }
    if (!Op->getType()->isPointerTy()) {
      // Non-pointers cannot influence Val
      continue;
    }
    if (ValNonEscaping || detail::isNonAddressTakenVariable(Op)) {
      // Op cannot reach anything but its own object, already ruled out above
      continue;
    }

    if (PT.mayPointsTo(OpObj, Val, Inst)) {
      return true;
    }
  }

  return false;
}

template <detail::HasMayPointsTo PTRef>
  requires detail::HasAsAbstractObject<PTRef>
auto SparsePointsToControlFlow<PTRef>::advanceToNextUserImplInternal(
    n_t Succ, o_t Val, PTRef PT, const llvm::DenseSet<o_t> &KnownGlobals)
    -> n_t {
  const auto *Save = Succ;
  while (!shouldKeepInst(Succ, Val, PT, KnownGlobals)) {
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

} // namespace psr
