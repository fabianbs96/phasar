/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_CONTROLFLOW_RESOLVER_COMPOSEDRESOLVER_H
#define PHASAR_CONTROLFLOW_RESOLVER_COMPOSEDRESOLVER_H

#include "phasar/ControlFlow/Resolver/Resolver.h"
#include "phasar/Utils/ByRef.h"

#include <type_traits>

namespace psr {

/// \brief A resolver that composes the two given resolvers
template <typename Res1T, typename Res2T,
          typename = std::enable_if_t<
              std::is_same_v<typename Res1T::n_t, typename Res2T::n_t> &&
              std::is_same_v<typename Res1T::f_t, typename Res2T::f_t>>>
struct ComposedResolver {
  using n_t = typename Res1T::n_t;
  using f_t = typename Res1T::f_t;

  [[no_unique_address]] Res1T Res1;
  [[no_unique_address]] Res2T Res2;

  /// Tries to resolve the given call with the first resolver, and uses the
  /// second resolver as fallback, if the first returns false.
  constexpr bool
  resolve(ByConstRef<n_t> Call,
          typename ResolverTraits<n_t, f_t>::FunctionSetTy &PossibleTargets) {
    if (Res1.resolve(Call, PossibleTargets)) {
      return true;
    }

    return Res2.resolve(Call, PossibleTargets);
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

/// Inline namespace that makes it possible to
/// '`using namespace psr::composed_resolver;`' to pull-in the operator|
/// without polluting the own namespace with the rest of the psr namespace.
inline namespace composed_resolver {

// clang-format off
/// Utility to make it possible to compose two resolvers with the pipe (|)
/// operator.
///
/// USAGE:
/// \code
/// auto Res = DirectCallResolver{} | RTAResolver{IRDB, VTP, TH} | SoundyFallbackResolver{IRDB};
/// \endcode
// clang-format on
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
