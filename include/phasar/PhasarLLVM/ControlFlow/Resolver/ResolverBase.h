/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVERBASE_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVERBASE_H

#include "phasar/ControlFlow/Resolver/GenericResolver.h"

#include <cassert>

namespace llvm {
class Instruction;
class CallBase;
class Function;
class DIType;
} // namespace llvm

namespace psr {
using LLVMResolverTraits =
    ResolverTraits<const llvm::CallBase *, const llvm::Function *>;

using LLVMGenericResolverRef =
    GenericResolverRef<const llvm::CallBase *, const llvm::Function *>;

using LLVMGenericResolver =
    GenericResolver<const llvm::CallBase *, const llvm::Function *>;

} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVERBASE_H
