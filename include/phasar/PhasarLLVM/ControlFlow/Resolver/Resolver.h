/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * Resolver.h
 *
 *  Created on: 20.07.2018
 *      Author: nicolas bellec
 */

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVER_H_
#define PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_RESOLVER_H_

#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverUtils.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "phasar/Utils/NonNullPtr.h"

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DerivedTypes.h"

#include <memory>
#include <string>

namespace psr {

/// A variant of F->hasAddressTaken() that is better suited for our use cases.
///
/// Especially, it filteres out global aliases.
[[nodiscard]] bool isAddressTakenFunction(const llvm::Function *F);

/// \brief A base class for call-target resolvers. Used to build call graphs.
///
/// Create a specific resolver by making a new class, inheriting this resolver
/// class and implementing the virtual functions as needed.
///
/// \deprecated Use GenericResolver and GenericResolverRef instead, as they
/// allow for resolver composition to form (custom) resolver pipelines.
///
class Resolver {
public:
  using FunctionSetTy = LLVMResolverTraits::FunctionSetTy;
  using n_t = const llvm::CallBase *;
  using f_t = const llvm::Function *;

  Resolver(NonNullPtr<const LLVMProjectIRDB> IRDB,
           NonNullPtr<const LLVMVFTableProvider> VTP);

  virtual ~Resolver() = default;

  [[deprecated("With the removal of DTAResolver, this is not used "
               "anymore")]] virtual void
  preCall(const llvm::Instruction *Inst);

  virtual void handlePossibleTargets(const llvm::CallBase *CallSite,
                                     FunctionSetTy &PossibleTargets);

  [[deprecated("With the removal of DTAResolver, this is not used "
               "anymore")]] virtual void
  postCall(const llvm::Instruction *Inst);

  [[nodiscard]] FunctionSetTy
  resolveIndirectCall(const llvm::CallBase *CallSite);

  [[deprecated("With the removal of DTAResolver, this is not used "
               "anymore")]] virtual void
  otherInst(const llvm::Instruction *Inst);

  [[nodiscard]] virtual std::string str() const = 0;

  /// Whether the ICFG needs to reconsider all dynamic call-sites once there
  /// have been changes through handlePossibleTargets().
  ///
  /// Make false for performance (may be less sound then)
  [[nodiscard]] virtual bool mutatesHelperAnalysisInformation() const noexcept {
    // Conservatively returns true. Override if possible
    return true;
  }

  [[nodiscard]] llvm::ArrayRef<const llvm::Function *>
  getAddressTakenFunctions();

  [[deprecated("Use psr::createDefaultResolverPipeline() "
               "instead")]] [[nodiscard]] static std::unique_ptr<Resolver>
  create(CallGraphAnalysisType Ty, const LLVMProjectIRDB *IRDB,
         const LLVMVFTableProvider *VTP, const DIBasedTypeHierarchy *TH,
         LLVMAliasInfoRef PT = nullptr);

protected:
  virtual void resolveVirtualCall(FunctionSetTy &PossibleTargets,
                                  const llvm::CallBase *CallSite) = 0;

  virtual void resolveFunctionPointer(FunctionSetTy &PossibleTargets,
                                      const llvm::CallBase *CallSite);

  const llvm::Function *
  getNonPureVirtualVFTEntry(const llvm::DIType *T, unsigned Idx,
                            const llvm::CallBase *CallSite,
                            const llvm::DIType *ReceiverType) {
    return psr::getNonPureVirtualVFTEntry(T, Idx, CallSite, *VTP, ReceiverType);
  }

  NonNullPtr<const LLVMProjectIRDB> IRDB;
  NonNullPtr<const LLVMVFTableProvider> VTP;
  std::optional<llvm::SmallVector<const llvm::Function *, 0>>
      AddressTakenFunctions{};
};
} // namespace psr

#endif
