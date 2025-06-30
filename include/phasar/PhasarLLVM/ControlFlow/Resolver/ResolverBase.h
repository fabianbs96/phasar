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
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"

#include <cassert>
#include <memory>
#include <type_traits>

namespace llvm {
class Instruction;
class CallBase;
class Function;
class DIType;
} // namespace llvm

namespace psr {

class GenericResolver {
public:
  using FunctionSetTy = resolver::FunctionSetTy;

  bool resolve(const llvm::CallBase *Call,
               resolver::FunctionSetTy &PossibleTargets) {
    assert(VT != nullptr);
    return VT->Resolve(Data, Call, PossibleTargets);
  }

  template <typename ConcreteResolverT,
            std::enable_if_t<IResolver<ConcreteResolverT>, int> = 0>
  GenericResolver(std::unique_ptr<ConcreteResolverT> Res) noexcept
      : Data(Res.release()), VT(&VTableFor<ConcreteResolverT>) {}

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
      : Data(Other.Data), VT(Other.VT) {
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

private:
  struct VTable {
    bool (*Resolve)(void *, const llvm::CallBase *, FunctionSetTy &);
    void (*Destroy)(const void *) noexcept;
  };

  template <typename ConcreteResolverT>
  static bool resolveThunk(void *Data, const llvm::CallBase *Call,
                           FunctionSetTy &PossibleTargets) {
    return static_cast<ConcreteResolverT *>(Data)->resolve(Call,
                                                           PossibleTargets);
  }
  template <typename ConcreteResolverT>
  static void destroyThunk(const void *Data) noexcept {
    delete static_cast<const ConcreteResolverT *>(Data);
  }

  template <typename ConcreteResolverT>
  static constexpr VTable VTableFor = {
      &resolveThunk<ConcreteResolverT>,
      &destroyThunk<ConcreteResolverT>,
  };

  void *Data{};
  const VTable *VT{};
};

class LLVMProjectIRDB;
class LLVMVFTableProvider;
class DIBasedTypeHierarchy;
enum class CallGraphAnalysisType;

[[nodiscard]] GenericResolver createDefaultResolverPipeline(
    CallGraphAnalysisType Ty, const LLVMProjectIRDB *IRDB,
    const LLVMVFTableProvider *VTP, const DIBasedTypeHierarchy *TH,
    LLVMAliasInfoRef PT = nullptr);

} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVERBASE_H
