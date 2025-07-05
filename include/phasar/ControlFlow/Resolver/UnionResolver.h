/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_CONTROLFLOW_RESOLVER_UNIONRESOLVER_H
#define PHASAR_CONTROLFLOW_RESOLVER_UNIONRESOLVER_H

#include "phasar/ControlFlow/Resolver/Resolver.h"
#include "phasar/Utils/ByRef.h"

#include <type_traits>

namespace psr {

/// \brief A resolver that combines the two given resolvers by computing the
/// set-union of their results. The second resolver (Res2T) has access to the
/// results of the first resolver.
template <typename Res1T, typename Res2T,
          typename = std::enable_if_t<
              std::is_same_v<typename Res1T::n_t, typename Res2T::n_t> &&
              std::is_same_v<typename Res1T::f_t, typename Res2T::f_t>>>
struct UnionResolver {
  using n_t = typename Res1T::n_t;
  using f_t = typename Res1T::f_t;

  [[no_unique_address]] Res1T Res1;
  [[no_unique_address]] Res2T Res2;

  constexpr bool
  resolve(ByConstRef<n_t> Call,
          typename ResolverTraits<n_t, f_t>::FunctionSetTy &PossibleTargets) {
    // Always invoke both resolvers
    bool Ret = Res1.resolve(Call, PossibleTargets);
    return Res2.resolve(Call, PossibleTargets) | Ret;
  }

  /// True, iff any of the both contained resolvers may mutate helper analysis
  /// information.
  [[nodiscard]] constexpr bool
  mutatesHelperAnalysisInformation() const noexcept {
    return ResolverTraits<n_t, f_t>::mutatesHelperAnalysisInformation(Res1) ||
           ResolverTraits<n_t, f_t>::mutatesHelperAnalysisInformation(Res2);
  }

  /// Asks both contained resolvers to modify helper analysis information, if
  /// possible/necessary
  constexpr void handlePossibleTargets(
      ByConstRef<n_t> CallSite,
      typename ResolverTraits<n_t, f_t>::FunctionSetTy &CalleeTargets) {
    ResolverTraits<n_t, f_t>::handlePossibleTargets(Res1, CallSite,
                                                    CalleeTargets);
    ResolverTraits<n_t, f_t>::handlePossibleTargets(Res2, CallSite,
                                                    CalleeTargets);
  }
};

template <typename Res1T, typename Res2T>
UnionResolver(Res1T, Res2T)
    -> UnionResolver<std::decay_t<Res1T>, std::decay_t<Res2T>>;

} // namespace psr

#endif // PHASAR_CONTROLFLOW_RESOLVER_UNIONRESOLVER_H
