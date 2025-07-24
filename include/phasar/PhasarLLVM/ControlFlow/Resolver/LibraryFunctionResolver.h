/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_LIBRARYFUNCTIONRESOLVER_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_LIBRARYFUNCTIONRESOLVER_H

#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverUtils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"

namespace psr {
struct LibraryFunctionResolver {
  using n_t = const llvm::CallBase *;
  using f_t = const llvm::Function *;

  bool resolve(const llvm::CallBase *Call,
               LLVMResolverTraits::FunctionSetTy &PossibleTargets);
};
} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_LIBRARYFUNCTIONRESOLVER_H
