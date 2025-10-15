/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_SOUNDYFALLBACKRESOLVER_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_SOUNDYFALLBACKRESOLVER_H

#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverUtils.h"
#include "phasar/PhasarLLVM/Utils/AddressTakenFunctions.h"

#include <cassert>

namespace psr {
class LLVMProjectIRDB;

/// A simple resolver that is meant as soundy fallback for the (hopefully rare)
/// case that a more precise resolver fails to resolve a particular call-site
struct SoundyFallbackResolver {
  using n_t = const llvm::CallBase *;
  using f_t = const llvm::Function *;

  SoundyFallbackResolver(const LLVMProjectIRDB &IRDB);

  SoundyFallbackResolver(AddressTakenFunctions ATF) noexcept
      : ATF(std::move(ATF)) {
    assert(!ATF.isNone());
  }

  AddressTakenFunctions ATF{};

  bool resolve(const llvm::CallBase *Call,
               LLVMResolverTraits::FunctionSetTy &PossibleTargets);
};
} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_SOUNDYFALLBACKRESOLVER_H
