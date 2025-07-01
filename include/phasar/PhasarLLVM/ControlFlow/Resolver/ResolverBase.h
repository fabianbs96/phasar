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
#include <type_traits>

namespace llvm {
class Instruction;
class CallBase;
class Function;
class DIType;
} // namespace llvm

namespace psr {

class GenericResolver;
class GenericResolverRef {
public:
  using FunctionSetTy = resolver::FunctionSetTy;

  template <typename ConcreteResolverT,
            std::enable_if_t<IResolver<ConcreteResolverT>, int> = 0>
  constexpr GenericResolverRef(ConcreteResolverT *Res) noexcept
      : Data(Res), VT(&VTableFor<ConcreteResolverT>) {
    assert(Res != nullptr);
  }
  template <typename ConcreteResolverT,
            std::enable_if_t<IResolver<ConcreteResolverT>, int> = 0>
  constexpr GenericResolverRef(
      std::reference_wrapper<ConcreteResolverT> Res) noexcept
      : Data(&Res.get()), VT(&VTableFor<ConcreteResolverT>) {
    assert(Res != nullptr);
  }

  template <typename ConcreteResolverT,
            std::enable_if_t<
                IResolver<ConcreteResolverT> &&
                    !std::is_base_of_v<GenericResolverRef, ConcreteResolverT>,
                int> = 0>
  constexpr GenericResolverRef(ConcreteResolverT &Res) noexcept = delete;

  // Prevent dangling references
  constexpr GenericResolverRef(GenericResolver &&) noexcept = delete;

  bool resolve(const llvm::CallBase *Call,
               resolver::FunctionSetTy &PossibleTargets) {
    assert(VT != nullptr);
    return VT->Resolve(Data, Call, PossibleTargets);
  }

  [[nodiscard]] bool mutatesHelperAnalysisInformation() const noexcept {
    assert(VT != nullptr);
    return VT->MutatesHelperAnalysisInformation(Data);
  }

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
    return resolver::mutatesHelperAnalysisInformation(
        *static_cast<const ConcreteResolverT *>(Data));
  }

  template <typename ConcreteResolverT>
  static void handlePossibleTargetsThunk(void *Data,
                                         const llvm::CallBase *CallSite,
                                         FunctionSetTy &CalleeTargets) {
    resolver::handlePossibleTargets(*static_cast<ConcreteResolverT *>(Data),
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

class GenericResolver : public GenericResolverRef {
public:
  template <typename ConcreteResolverT,
            std::enable_if_t<IResolver<ConcreteResolverT>, int> = 0>
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

  [[nodiscard]] constexpr GenericResolverRef get() & noexcept {
    return static_cast<GenericResolverRef>(*this);
  }
  constexpr GenericResolverRef get() && noexcept = delete;
};

} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVERBASE_H
