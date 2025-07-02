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

#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverUtils.h"

#include <cassert>
#include <functional>
#include <memory>

namespace llvm {
class Instruction;
class CallBase;
class Function;
class DIType;
} // namespace llvm

namespace psr {

class GenericResolver;

/// \brief A type-erased non-owning reference to a call-target resolver. Used to
/// build call-graphs.
///
/// Create a specific resolver by making a new class/struct implementing the
/// function resolve(const llvm::CallBase *, FunctionSetTy &)->bool;
class [[gsl::Pointer]] GenericResolverRef {
public:
  using FunctionSetTy = LLVMResolverTraits::FunctionSetTy;

  /// Create a type-erased reference from a pointer to a concrete resolver
  ///
  /// \pre Requires that the provided pointer is non-null and refers to a valid
  /// resolver
  template <typename ConcreteResolverT,
            std::enable_if_t<LLVMResolverTraits::IResolver<ConcreteResolverT>,
                             int> = 0>
  constexpr GenericResolverRef(ConcreteResolverT *Res) noexcept
      : Data(Res), VT(&VTableFor<ConcreteResolverT>) {
    assert(Res != nullptr);
  }

  /// Create a type-erased reference from a std::reference_wrapper to a concrete
  /// resolver
  template <typename ConcreteResolverT,
            std::enable_if_t<LLVMResolverTraits::IResolver<ConcreteResolverT>,
                             int> = 0>
  constexpr GenericResolverRef(
      std::reference_wrapper<ConcreteResolverT> Res) noexcept
      : Data(&Res.get()), VT(&VTableFor<ConcreteResolverT>) {
    assert(Res != nullptr);
  }

  /// Prevent implicit casting from references to emphasize that this class
  /// models a non-owning reference.
  template <typename ConcreteResolverT,
            std::enable_if_t<
                LLVMResolverTraits::IResolver<ConcreteResolverT> &&
                    !std::is_base_of_v<GenericResolverRef, ConcreteResolverT>,
                int> = 0>
  constexpr GenericResolverRef(ConcreteResolverT &Res) noexcept = delete;

  /// Prevent dangling references
  constexpr GenericResolverRef(GenericResolver &&) noexcept = delete;

  /// Tries to resolve the given (indirect) Call.
  ///
  /// \param Call The call to resolve. Implementations can assume that this
  /// parameter is non-null and points to a valid llvm::CallBase
  /// \param PossibleTargets A set, where the possible call-targets should be
  /// written.
  /// \returns True, if the call could be resolved, false otherwise.
  bool resolve(const llvm::CallBase *Call,
               LLVMResolverTraits::FunctionSetTy &PossibleTargets) {
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
  void handlePossibleTargets(const llvm::CallBase *CallSite,
                             FunctionSetTy &CalleeTargets) {
    assert(VT != nullptr);
    VT->HandlePossibleTargets(Data, CallSite, CalleeTargets);
  }

private:
  friend class GenericResolver;

  struct VTable {
    bool (*Resolve)(void *, const llvm::CallBase *, FunctionSetTy &);
    bool (*MutatesHelperAnalysisInformation)(const void *) noexcept;
    void (*HandlePossibleTargets)(void *, const llvm::CallBase *CallSite,
                                  FunctionSetTy &CalleeTargets);
    void (*Destroy)(const void *) noexcept;
  };

  constexpr GenericResolverRef(void *Data, const VTable *VT) noexcept
      : Data(Data), VT(VT) {}

  template <typename ConcreteResolverT>
  static bool resolveThunk(void *Data, const llvm::CallBase *Call,
                           FunctionSetTy &PossibleTargets) {
    return static_cast<ConcreteResolverT *>(Data)->resolve(Call,
                                                           PossibleTargets);
  }

  template <typename ConcreteResolverT>
  static bool mutatesHelperAnalysisInformationThunk(const void *Data) noexcept {
    return LLVMResolverTraits::mutatesHelperAnalysisInformation(
        *static_cast<const ConcreteResolverT *>(Data));
  }

  template <typename ConcreteResolverT>
  static void handlePossibleTargetsThunk(void *Data,
                                         const llvm::CallBase *CallSite,
                                         FunctionSetTy &CalleeTargets) {
    LLVMResolverTraits::handlePossibleTargets(
        *static_cast<ConcreteResolverT *>(Data), CallSite, CalleeTargets);
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
class [[clang::trivial_abi, gsl::Owner]] GenericResolver
    : public GenericResolverRef {
public:
  /// Create a type-erased GenericResolver from a std::unique_ptr to a concrete
  /// resolver.
  template <typename ConcreteResolverT,
            std::enable_if_t<LLVMResolverTraits::IResolver<ConcreteResolverT>,
                             int> = 0>
  GenericResolver(std::unique_ptr<ConcreteResolverT> Res) noexcept
      : GenericResolverRef(Res.release()) {}

  ~GenericResolver() {
    if (VT) {
      VT->Destroy(Data);
    }
#ifndef NDEBUG
    Data = nullptr;
    VT = nullptr;
#endif
  }

  GenericResolver(const GenericResolver &) = delete;
  GenericResolver &operator=(const GenericResolver &) = delete;

  constexpr GenericResolver(GenericResolver &&Other) noexcept
      : GenericResolverRef(Other.Data, Other.VT) {
    Other.Data = nullptr;
    Other.VT = nullptr;
  }

  void swap(GenericResolver &Other) noexcept {
    std::swap(Data, Other.Data);
    std::swap(VT, Other.VT);
  }

  GenericResolver &operator=(GenericResolver &&Other) noexcept {
    GenericResolver(std::move(Other)).swap(*this);
    return *this;
  }

  /// Get a GenericResolverRef that refers to the owned type-erased resolver.
  ///
  /// \attention You must make sure that this owning GenericResolver outlives
  /// all uses of the returned GenericResolverRef.
  [[nodiscard]] constexpr GenericResolverRef get() & noexcept {
    return static_cast<GenericResolverRef>(*this);
  }
  constexpr GenericResolverRef get() && noexcept = delete;
};

} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVERBASE_H
