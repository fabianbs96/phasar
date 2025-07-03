/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVERUTILS_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVERUTILS_H

#include "phasar/ControlFlow/Resolver/Resolver.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"

#include <optional>
#include <string>

namespace llvm {
class Instruction;
class CallBase;
class Function;
class DIType;
} // namespace llvm

namespace psr {
class LLVMProjectIRDB;
class LLVMVFTableProvider;
class DIBasedTypeHierarchy;
enum class CallGraphAnalysisType;

using LLVMResolverTraits =
    psr::ResolverTraits<const llvm::CallBase *, const llvm::Function *>;

/// Assuming that `CallSite` is a virtual call through a vtable, retrieves the
/// index in the vtable of the virtual function called.
[[nodiscard]] std::optional<unsigned>
getVFTIndex(const llvm::CallBase *CallSite);

/// Assuming that `CallSite` is a call to a non-static member function,
/// retrieves the type of the receiver. Returns nullptr, if the receiver-type
/// could not be extracted
[[nodiscard]] const llvm::DIType *
getReceiverType(const llvm::CallBase *CallSite);

/// Assuming that `CallSite` is a virtual call, where `Idx` is retrieved through
/// `getVFTIndex()` and `T` through `getReceiverType()`
[[nodiscard]] const llvm::Function *
getNonPureVirtualVFTEntry(const llvm::DIType *T, unsigned Idx,
                          const llvm::CallBase *CallSite,
                          const psr::LLVMVFTableProvider &VTP);

[[nodiscard]] std::string getReceiverTypeName(const llvm::CallBase *CallSite);

/// Checks whether the signature of `DestFun` matches the required withature of
/// `CallSite`, such that `DestFun` qualifies as callee-candidate, if `CallSite`
/// is an indirect/virtual call.
[[nodiscard]] bool isConsistentCall(const llvm::CallBase *CallSite,
                                    const llvm::Function *DestFun);

[[nodiscard]] bool isVirtualCall(const llvm::Instruction *Inst,
                                 const LLVMVFTableProvider &VTP);
} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVERUTILS_H
