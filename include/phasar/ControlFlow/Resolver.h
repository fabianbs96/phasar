/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_CONTROLFLOW_RESOLVER_RESOLVER_H
#define PHASAR_CONTROLFLOW_RESOLVER_RESOLVER_H

#include "phasar/Utils/Macros.h"

#include "llvm/ADT/DenseSet.h"

#if __cpp_concepts >= 201907L
#include <concepts>
#endif

namespace psr {
template <typename N, typename F> struct ResolverTraits {
  using FunctionSetTy = llvm::SmallDenseSet<F, 4>;

  PSR_DECLARE_HAS_MEMBER_FN(resolve, std::declval<N>(),
                            std::declval<FunctionSetTy &>());

  PSR_DECLARE_HAS_MEMBER_FN(handlePossibleTargets, std::declval<N>(),
                            std::declval<FunctionSetTy &>());
  PSR_DECLARE_HAS_MEMBER_FN(mutatesHelperAnalysisInformation);

#if __cpp_concepts >= 201907L
  template <typename T>
  concept IResolver = requires(T &Res, FunctionSetTy &FSet, N CB) {
    { Res.resolve(CB, FSet) } -> std::convertible_to<bool>;
  };

#else

  template <typename T> PSR_CONCEPT IResolver = has_resolve_v<T>;

#endif

  template <typename ResolverT>
  static constexpr bool
  mutatesHelperAnalysisInformation(const ResolverT &Res) noexcept {
    if constexpr (has_mutatesHelperAnalysisInformation_v<const ResolverT>) {
      return Res.mutatesHelperAnalysisInformation();
    } else {
      // Sound fallback
      return has_handlePossibleTargets_v<ResolverT>;
    }
  }

  template <typename ResolverT>
  static constexpr void
  handlePossibleTargets(ResolverT &Res, N CallSite,
                        FunctionSetTy &CalleeTargets) noexcept {
    if constexpr (has_handlePossibleTargets_v<ResolverT>) {
      Res.handlePossibleTargets(CallSite, CalleeTargets);
    }
    // else do nothing
  }
};

} // namespace psr

#endif // PHASAR_CONTROLFLOW_RESOLVER_RESOLVER_H
