/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_COMPOSEDRESOLVER_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_COMPOSEDRESOLVER_H

#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverUtils.h"

#include <type_traits>

namespace psr {
template <typename Res1T, typename Res2T> struct ComposedResolver {
  [[no_unique_address]] Res1T Res1;
  [[no_unique_address]] Res2T Res2;

  constexpr bool resolve(const llvm::CallBase *Call,
                         resolver::FunctionSetTy &PossibleTargets) {
    if (Res1.resolve(Call, PossibleTargets)) {
      return true;
    }

    return Res2.resolve(Call, PossibleTargets);
  }

  [[nodiscard]] constexpr bool
  mutatesHelperAnalysisInformation() const noexcept {
    return resolver::mutatesHelperAnalysisInformation(Res1) ||
           resolver::mutatesHelperAnalysisInformation(Res2);
  }

  constexpr void handlePossibleTargets(const llvm::CallBase *CallSite,
                                       resolver::FunctionSetTy &CalleeTargets) {
    resolver::handlePossibleTargets(Res1, CallSite, CalleeTargets);
    resolver::handlePossibleTargets(Res2, CallSite, CalleeTargets);
  }
};

inline namespace composed_resolver {
template <typename R1, typename R2>
constexpr
#if __cpp_concepts >= 201907L
    requires(IResolver<R1> &&IResolver<R2>) ComposedResolver<R1, R2>
#else
    std::enable_if_t<IResolver<R1> && IResolver<R2>, ComposedResolver<R1, R2>>
#endif
    operator|(R1 Res1, R2 Res2) {
  return {std::move(Res1), std::move(Res2)};
}
} // namespace composed_resolver

} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_COMPOSEDRESOLVER_H
