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

#include <memory>
#include <string>

namespace psr {

/// \brief A base class for call-target resolvers. Used to build call graphs.
///
/// Create a specific resolver by making a new class, inheriting this resolver
/// class and implementing the virtual functions as needed.
///
/// \deprecated Use GenericResolver and GenericResolverRef instead, as they
/// allow for resolver composition to form (custom) resolver pipelines.
///
class Resolver {
protected:
  const LLVMProjectIRDB *IRDB;
  const LLVMVFTableProvider *VTP;

  const llvm::Function *
  getNonPureVirtualVFTEntry(const llvm::DIType *T, unsigned Idx,
                            const llvm::CallBase *CallSite) {
    if (!VTP) {
      return nullptr;
    }
    return psr::getNonPureVirtualVFTEntry(T, Idx, CallSite, *VTP);
  }

public:
  using FunctionSetTy = LLVMResolverTraits::FunctionSetTy;
  using n_t = const llvm::CallBase *;
  using f_t = const llvm::Function *;

  Resolver(const LLVMProjectIRDB *IRDB, const LLVMVFTableProvider *VTP);

  virtual ~Resolver() = default;

  virtual void preCall(const llvm::Instruction *Inst);

  virtual void handlePossibleTargets(const llvm::CallBase *CallSite,
                                     FunctionSetTy &PossibleTargets);

  virtual void postCall(const llvm::Instruction *Inst);

  [[nodiscard]] FunctionSetTy
  resolveIndirectCall(const llvm::CallBase *CallSite);

  [[nodiscard]] virtual FunctionSetTy
  resolveVirtualCall(const llvm::CallBase *CallSite) = 0;

  [[nodiscard]] virtual FunctionSetTy
  resolveFunctionPointer(const llvm::CallBase *CallSite);

  virtual void otherInst(const llvm::Instruction *Inst);

  [[nodiscard]] virtual std::string str() const = 0;

  [[nodiscard]] virtual bool mutatesHelperAnalysisInformation() const noexcept {
    // Conservatively returns true. Override if possible
    return true;
  }

  [[deprecated("Use psr::createDefaultResolverPipeline() instead")]]
  static std::unique_ptr<Resolver>
  create(CallGraphAnalysisType Ty, const LLVMProjectIRDB *IRDB,
         const LLVMVFTableProvider *VTP, const DIBasedTypeHierarchy *TH,
         LLVMAliasInfoRef PT = nullptr);
};
} // namespace psr

#endif
