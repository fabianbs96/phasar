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

#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "phasar/Utils/Macros.h"

#include "llvm/ADT/DenseSet.h"

#include <optional>
#include <string>

#if __cpp_concepts >= 201907L
#include <concepts>
#endif

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

namespace resolver {
using FunctionSetTy = llvm::SmallDenseSet<const llvm::Function *, 4>;
} // namespace resolver

#if __cpp_concepts >= 201907L
template <typename T>
concept IResolver =
    requires(T &Res, resolver::FunctionSetTy &FSet, const llvm::CallBase *CB) {
      { Res.resolve(CB, FSet) } -> std::convertible_to<bool>;
    };

#else

namespace detail {
PSR_DECLARE_HAS_MEMBER_FN(resolve, std::declval<const llvm::CallBase *>(),
                          std::declval<resolver::FunctionSetTy &>());

} // namespace detail

template <typename T> PSR_CONCEPT IResolver = detail::has_resolve_v<T>;

#endif

namespace detail {
PSR_DECLARE_HAS_MEMBER_FN(handlePossibleTargets,
                          std::declval<const llvm::CallBase *>(),
                          std::declval<resolver::FunctionSetTy &>());
PSR_DECLARE_HAS_MEMBER_FN(mutatesHelperAnalysisInformation);
} // namespace detail

namespace resolver {
template <typename ResolverT>
constexpr bool mutatesHelperAnalysisInformation(const ResolverT &Res) noexcept {
  if constexpr (detail::has_mutatesHelperAnalysisInformation_v<
                    const ResolverT>) {
    return Res.mutatesHelperAnalysisInformation();
  } else {
    // Sound fallback
    return detail::has_handlePossibleTargets_v<ResolverT>;
  }
}

template <typename ResolverT>
constexpr void handlePossibleTargets(ResolverT &Res,
                                     const llvm::CallBase *CallSite,
                                     FunctionSetTy &CalleeTargets) noexcept {
  if constexpr (detail::has_handlePossibleTargets_v<ResolverT>) {
    Res.handlePossibleTargets(CallSite, CalleeTargets);
  }
  // else do nothing
}
} // namespace resolver

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
