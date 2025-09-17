/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * CHAResolver.h
 *
 *  Created on: 20.07.2018
 *      Author: nicolas bellec
 */

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_CHARESOLVER_H_
#define PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_CHARESOLVER_H_

#include "phasar/PhasarLLVM/ControlFlow/Resolver/Resolver.h"
#include "phasar/Utils/MaybeUniquePtr.h"
#include "phasar/Utils/NonNullPtr.h"

namespace psr {
class DIBasedTypeHierarchy;

/// \brief A resolver that performs Class Hierarchy Analysis to resolve calls
/// to C++ virtual functions. Requires debug information.
class CHAResolver : public Resolver {
public:
  CHAResolver(NonNullPtr<const LLVMProjectIRDB> IRDB,
              NonNullPtr<const LLVMVFTableProvider> VTP,
              const DIBasedTypeHierarchy *TH);

  CHAResolver(CHAResolver &&) noexcept;
  CHAResolver &operator=(CHAResolver &&) noexcept;

  CHAResolver(const CHAResolver &) = delete;
  CHAResolver &operator=(const CHAResolver &) = delete;

  // Deleting an incomplete type (LLVMTypeHierarchy) is UB, so instantiate the
  // dtor in CHAResolver.cpp
  ~CHAResolver() override;

  void resolveVirtualCall(FunctionSetTy &PossibleTargets,
                          const llvm::CallBase *CallSite) override;

  bool resolve(const llvm::CallBase *Call, FunctionSetTy &PossibleTargets);

  [[nodiscard]] std::string str() const override;

  [[nodiscard]] bool
  mutatesHelperAnalysisInformation() const noexcept override {
    return false;
  }

protected:
  MaybeUniquePtr<const DIBasedTypeHierarchy, true> TH;
};
} // namespace psr

#endif
