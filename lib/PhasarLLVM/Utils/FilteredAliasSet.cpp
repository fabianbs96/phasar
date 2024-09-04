#include "phasar/PhasarLLVM/Utils/FilteredAliasSet.h"

#include "llvm/IR/Argument.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"

namespace psr {

template <bool MustAlias>
void foreachFilteredAliasSetImpl(
    const llvm::Value *Fact, const llvm::Instruction *At,
    llvm::function_ref<void(const llvm::Value *)> WithAlias,
    FilteredAliasSet::alias_info_ref_t PT) {

  const auto *Base = Fact->stripPointerCastsAndAliases();

  static constexpr auto GetFunction =
      [](const llvm::Value *V) -> const llvm::Function * {
    if (const auto *Inst = llvm::dyn_cast<llvm::Instruction>(V)) {
      return Inst->getFunction();
    }
    if (const auto *Arg = llvm::dyn_cast<llvm::Argument>(V)) {
      return Arg->getParent();
    }
    return nullptr;
  };

  // If mustNoalias is false, then p1 and p2 may alias. If mustNoalias is true,
  // then p1 and p2 definitely are not aliases.
  static constexpr auto MustNoalias = [](const llvm::Value *P1,
                                         const llvm::Value *P2) {
    if (P1 == P2) {
      return false;
    }
    assert(P1);
    assert(P2);
    if (const auto *Alloca1 = llvm::dyn_cast<llvm::AllocaInst>(P1)) {
      if (llvm::isa<llvm::GlobalVariable>(P2)) {
        return true;
      }
      if (const auto *Alloca2 = llvm::dyn_cast<llvm::AllocaInst>(P2)) {
        return !Alloca1->getAllocatedType()->isPointerTy() &&
               !Alloca2->getAllocatedType()->isPointerTy();
      }
    } else if (const auto *Glob1 = llvm::dyn_cast<llvm::GlobalVariable>(P1)) {
      if (llvm::isa<llvm::AllocaInst>(P2) || Glob1->isConstant()) {
        return true;
      }
      if (const auto *Glob2 = llvm::dyn_cast<llvm::GlobalVariable>(P2)) {
        return true; // approximation
      }
    } else if (const auto *Glob2 = llvm::dyn_cast<llvm::GlobalVariable>(P2)) {
      return Glob2->isConstant();
    }

    return false;
  };

  const auto *FactFun = At ? At->getFunction() : GetFunction(Fact);

  PT(Fact, [&](const llvm::Value *Alias) {
    if (FactFun) {
      // Skip inter-procedural aliases
      const auto *AliasFun = GetFunction(Alias);
      if (FactFun != AliasFun && AliasFun) {
        return;
      }
    }
    if (Fact == Alias) {
      WithAlias(Alias);
      return;
    }

    const auto *AliasBase = Alias->stripPointerCastsAndAliases();
    if (MustNoalias(Base, AliasBase)) {
      return;
    }

    // bool IsMatching = false;
    // auto Res = PT.alias(Fact, Alias, At);

    // if constexpr (MustAlias) {
    //   IsMatching = Res == psr::AliasResult::MustAlias;
    // } else {
    //   IsMatching = Res != psr::AliasResult::NoAlias;
    // }
    // if (IsMatching) {
    //   WithAlias(Alias);
    // }
    WithAlias(Alias);

    if (const auto *Load = llvm::dyn_cast<llvm::LoadInst>(Alias)) {
      WithAlias(Load->getPointerOperand());
    }
  });
}

auto FilteredAliasSet::getAliasSet(d_t Val, n_t At) -> container_type {
  container_type Ret;
  foreachFilteredAliasSetImpl<false>(
      Val, At, [&Ret](d_t Alias) { Ret.insert(Alias); }, PT);
  return Ret;
}
// auto FilteredAliasSet::getMustAliasSet(d_t Val, n_t At) -> container_type {
//   container_type Ret;
//   foreachFilteredAliasSetImpl<true>(
//       Val, At, [&Ret](d_t Alias) { Ret.insert(Alias); }, PT);
//   return Ret;
// }

void FilteredAliasSet::foreachAlias(d_t Fact, n_t At,
                                    llvm::function_ref<void(d_t)> WithAlias) {
  foreachFilteredAliasSetImpl<false>(Fact, At, WithAlias, PT);
}
// void FilteredAliasSet::foreachdMustAlias(
//     d_t Fact, n_t At, llvm::function_ref<void(d_t)> WithAlias) {
//   foreachFilteredAliasSetImpl<true>(Fact, At, WithAlias, PT);
// }
} // namespace psr
