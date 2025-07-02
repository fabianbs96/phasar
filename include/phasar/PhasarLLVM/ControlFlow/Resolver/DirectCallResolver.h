/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_DIRECTCALLRESOLVER_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_DIRECTCALLRESOLVER_H

#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverUtils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/Casting.h"

namespace psr {
/// Simple resolver that only handles direct calls and does not attempt to
/// resolve indirect calls (e.g., a C++ virtual call, or a call through a
/// function pointer)
struct DirectCallResolver {

  bool resolve(const llvm::CallBase *Call,
               LLVMResolverTraits::FunctionSetTy &PossibleTargets) {
    const auto *CalledOp =
        Call->getCalledOperand()->stripPointerCastsAndAliases();
    if (const auto *Target = llvm::dyn_cast<llvm::Function>(CalledOp)) {
      PossibleTargets.insert(Target);
      return true;
    }

    return false;
  }
};
} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_DIRECTCALLRESOLVER_H
