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

#include <cassert>

namespace psr {
class LLVMProjectIRDB;

class SoundyFallbackResolver {
public:
  constexpr SoundyFallbackResolver(const LLVMProjectIRDB *IRDB) : IRDB(IRDB) {
    assert(IRDB != nullptr);
  }

  bool resolve(const llvm::CallBase *Call,
               resolver::FunctionSetTy &PossibleTargets);

private:
  const LLVMProjectIRDB *IRDB{};
};
} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_SOUNDYFALLBACKRESOLVER_H
