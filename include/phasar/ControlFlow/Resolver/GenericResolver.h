/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_CONTROLFLOW_RESOLVER_GENERICRESOLVER_H
#define PHASAR_CONTROLFLOW_RESOLVER_GENERICRESOLVER_H

#include "phasar/ControlFlow/Resolver/Resolver.h"
#include "phasar/Utils/ByRef.h"

#include <memory>
#include <type_traits>

namespace psr {

template <typename N, typename F> class GenericResolver;

/// \brief A type-erased non-owning reference to a call-target resolver. Used to
/// build call-graphs.
///
/// Create a specific resolver by making a new class/struct implementing the
/// function resolve(const llvm::CallBase *, FunctionSetTy &)->bool;
template <typename N, typename F> class [[gsl::Pointer]] GenericResolverRef {
public:
  using Traits = ResolverTraits<N, F>;
  using FunctionSetTy = typename Traits::FunctionSetTy;
  using n_t = N;
  using f_t = F;

  /// Create a type-erased reference from a pointer to a concrete resolver
  ///
  /// \pre Requires that the provided pointer is non-null and refers to a valid
  /// resolver
  template <typename ConcreteResolverT,
            std::enable_if_t<IResolverNF<ConcreteResolverT, n_t, f_t>, int> = 0>
  constexpr GenericResolverRef(ConcreteResolverT *Res) noexcept
      : GenericResolverRef(Res, std::true_type{}) {}

  /// Create a type-erased reference from a std::reference_wrapper to a concrete
  /// resolver
  template <typename ConcreteResolverT,
            std::enable_if_t<IResolverNF<ConcreteResolverT, n_t, f_t>, int> = 0>
  constexpr GenericResolverRef(
      std::reference_wrapper<ConcreteResolverT> Res) noexcept
      : Data(&Res.get()), VT(&VTableFor<ConcreteResolverT>) {
    assert(Res != nullptr);
  }

  /// Prevent implicit casting from references to emphasize that this class
  /// models a non-owning reference.
  template <typename ConcreteResolverT,
            std::enable_if_t<
                !std::is_pointer_v<ConcreteResolverT> &&
                    IResolverNF<ConcreteResolverT, n_t, f_t> &&
                    !std::is_base_of_v<GenericResolverRef, ConcreteResolverT>,
                int> = 0>
  constexpr GenericResolverRef(ConcreteResolverT &Res) noexcept = delete;

  /// Prevent dangling references
  constexpr GenericResolverRef(GenericResolver<N, F> &&) noexcept = delete;

  /// Tries to resolve the given (indirect) Call, storing the possible callee
  /// targets in PossibleTargets.
  ///
  /// \param Call The call to resolve. Implementations can assume that this
  /// parameter is non-null and points to a valid llvm::CallBase
  /// \param PossibleTargets A set, where the possible call-targets should be
  /// written.
  /// \returns True, if the call could be resolved, false otherwise.
  bool resolve(ByConstRef<n_t> Call, FunctionSetTy &PossibleTargets) {
    assert(VT != nullptr);
    return VT->Resolve(Data, Call, PossibleTargets);
  }

  /// True, iff this resolver may implement some logic to modify information
  /// from the HelperAnalyses, e.g., refining alias-information.
  ///
  /// \note You do not need to provide this function. If absent, it will be
  /// defaulted based on the presence of the function handlePossibleTargets()
  [[nodiscard]] bool mutatesHelperAnalysisInformation() const noexcept {
    assert(VT != nullptr);
    return VT->MutatesHelperAnalysisInformation(Data);
  }

  /// Function to mutate information from the HelperAnalyses based on the
  /// information given. If this function does something,
  /// mutatesHelperAnalysisInformation() should return true, if provided.
  ///
  /// \note You do not need to provide this function. If absent, it will be
  /// defaulted to just doing nothing.
  ///
  /// \param CallSite The call-site where the CalleeTargets *new* possible
  /// targets have been found.
  /// Implementations can assume that this parameter is non-null and points to a
  /// valid llvm::CallBase
  /// \param CalleeTargets A set of *new* possible targets
  /// that have been found for the given CallSite
  void handlePossibleTargets(ByConstRef<n_t> CallSite,
                             FunctionSetTy &CalleeTargets) {
    assert(VT != nullptr);
    VT->HandlePossibleTargets(Data, CallSite, CalleeTargets);
  }

private:
  template <typename NN, typename FF> friend class GenericResolver;

  struct VTable {
    bool (*Resolve)(void *, ByConstRef<n_t>, FunctionSetTy &);
    bool (*MutatesHelperAnalysisInformation)(const void *) noexcept;
    void (*HandlePossibleTargets)(void *, ByConstRef<n_t> CallSite,
                                  FunctionSetTy &CalleeTargets);
    void (*Destroy)(const void *) noexcept;
  };

  template <typename ConcreteResolverT, bool ExpectNonNullRes>
  constexpr GenericResolverRef(
      ConcreteResolverT *Res,
      std::bool_constant<ExpectNonNullRes> /*unused*/) noexcept
      : Data(Res), VT(&VTableFor<ConcreteResolverT>) {
    if constexpr (ExpectNonNullRes) {
      assert(Res != nullptr);
    } else if (!Res) {
      VT = nullptr;
    }
  }

  template <typename ConcreteResolverT>
  static bool resolveThunk(void *Data, ByConstRef<n_t> Call,
                           FunctionSetTy &PossibleTargets) {
    return static_cast<ConcreteResolverT *>(Data)->resolve(Call,
                                                           PossibleTargets);
  }

  template <typename ConcreteResolverT>
  static bool mutatesHelperAnalysisInformationThunk(const void *Data) noexcept {
    return Traits::mutatesHelperAnalysisInformation(
        *static_cast<const ConcreteResolverT *>(Data));
  }

  template <typename ConcreteResolverT>
  static void handlePossibleTargetsThunk(void *Data, ByConstRef<n_t> CallSite,
                                         FunctionSetTy &CalleeTargets) {
    Traits::handlePossibleTargets(*static_cast<ConcreteResolverT *>(Data),
                                  CallSite, CalleeTargets);
  }

  template <typename ConcreteResolverT>
  static void destroyThunk(const void *Data) noexcept {
    delete static_cast<const ConcreteResolverT *>(Data);
  }

  template <typename ConcreteResolverT>
  static constexpr VTable VTableFor = {
      &resolveThunk<ConcreteResolverT>,
      &mutatesHelperAnalysisInformationThunk<ConcreteResolverT>,
      &handlePossibleTargetsThunk<ConcreteResolverT>,
      &destroyThunk<ConcreteResolverT>,
  };

  void *Data{};
  const VTable *VT{};
};

/// \brief Owning variant of GenericResolverRef.
template <typename N, typename F>
class [[clang::trivial_abi, gsl::Owner]] GenericResolver
    : public GenericResolverRef<N, F> {
public:
  using typename GenericResolverRef<N, F>::f_t;
  using typename GenericResolverRef<N, F>::n_t;
  using typename GenericResolverRef<N, F>::FunctionSetTy;
  using typename GenericResolverRef<N, F>::Traits;

  /// Create a type-erased GenericResolver from a std::unique_ptr to a concrete
  /// resolver.
  template <typename ConcreteResolverT,
            std::enable_if_t<IResolverNF<ConcreteResolverT, n_t, f_t>, int> = 0>
  GenericResolver(std::unique_ptr<ConcreteResolverT> Res) noexcept
      : GenericResolverRef<N, F>(Res.release(), std::false_type{}) {}

  ~GenericResolver() {
    if (this->VT) {
      this->VT->Destroy(this->Data);
    }
#ifndef NDEBUG
    this->Data = nullptr;
    this->VT = nullptr;
#endif
  }

  GenericResolver(const GenericResolver &) = delete;
  GenericResolver &operator=(const GenericResolver &) = delete;

  constexpr GenericResolver(GenericResolver &&Other) noexcept
      : GenericResolverRef<N, F>(Other.Data, Other.VT) {
    Other.Data = nullptr;
    Other.VT = nullptr;
  }

  void swap(GenericResolver &Other) noexcept {
    std::swap(this->Data, Other.Data);
    std::swap(this->VT, Other.VT);
  }

  GenericResolver &operator=(GenericResolver &&Other) noexcept {
    GenericResolver(std::move(Other)).swap(*this);
    return *this;
  }

  /// Get a GenericResolverRef that refers to the owned type-erased resolver.
  ///
  /// \attention You must make sure that this owning GenericResolver outlives
  /// all uses of the returned GenericResolverRef.
  /// \pre This GenericResolver has not been moved from and was initialized with
  /// a non-null (unique-)pointer
  [[nodiscard]] constexpr GenericResolverRef<N, F> get() & noexcept {
    return static_cast<GenericResolverRef<N, F>>(*this);
  }
  constexpr GenericResolverRef<N, F> get() && noexcept = delete;
};

} // namespace psr

#endif // PHASAR_CONTROLFLOW_RESOLVER_GENERICRESOLVER_H
