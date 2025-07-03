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

#include <type_traits>

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

template <typename T>
using ResolverTraitsFor = ResolverTraits<typename T::n_t, typename T::f_t>;

#if __cpp_concepts >= 201907L
template <typename T>
concept IResolver =
    requires(T &Res, typename ResolverTraitsFor<T>::FunctionSetTy &FSet,
             typename T::n_t CB) {
      typename T::n_t;
      typename T::f_t;
      { Res.resolve(CB, FSet) } -> std::convertible_to<bool>;
    };

template <typename T, typename N, typename F>
concept IResolverNF = IResolver<T> && std::same_as<typename T::n_t, N> &&
                      std::same_as<typename T::f_t, F>;

#else

template <typename T, typename N, typename F>
PSR_CONCEPT IResolverNF = ResolverTraits<N, F>::template has_resolve_v<T>;

namespace detail {
template <typename T, typename = void>
struct IResolverImpl : std::false_type {};
template <typename T>
struct IResolverImpl<
    T, std::enable_if_t<ResolverTraitsFor<T>::template has_resolve_v<T>>>
    : std::true_type {};
} // namespace detail

template <typename T> PSR_CONCEPT IResolver = detail::IResolverImpl<T>::value;

#endif

} // namespace psr

#endif // PHASAR_CONTROLFLOW_RESOLVER_RESOLVER_H
