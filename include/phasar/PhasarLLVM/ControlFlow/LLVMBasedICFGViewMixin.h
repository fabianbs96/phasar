#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/ControlFlow/CFGBase.h"
#include "phasar/ControlFlow/CallGraph.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/Utils/LLVMBasedContainerConfig.h"
#include "phasar/Utils/NonNullPtr.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include <cstddef>
#include <vector>

namespace llvm {
class raw_ostream;
} // namespace llvm

namespace psr {
class LLVMBasedICFG;

/// \brief CRTP mixin for a non-owning view onto an existing LLVMBasedICFG:
/// forwards every ICFGBase query to it unchanged. Used as a base by ICFG
/// wrappers (e.g. sparsified ones) whose only difference from LLVMBasedICFG
/// itself is how getSparseCFG/advanceToNextUser are implemented.
template <typename Derived> class LLVMBasedICFGViewMixin {
public:
  using n_t = typename CFGTraits<Derived>::n_t;
  using f_t = typename CFGTraits<Derived>::f_t;

  explicit LLVMBasedICFGViewMixin(
      const LLVMBasedICFG *ICF PSR_LIFETIMEBOUND) noexcept
      : ICF(&assertNotNull(ICF)) {}

  explicit constexpr LLVMBasedICFGViewMixin(
      NonNullPtr<const LLVMBasedICFG> ICF PSR_LIFETIMEBOUND) noexcept
      : ICF(ICF) {}

  // To make the IDESolver happy...
  constexpr operator const LLVMBasedICFG &() const noexcept PSR_LIFETIMEBOUND {
    return *ICF;
  }

protected:
  [[nodiscard]] FunctionRange getAllFunctionsImpl() const {
    return ICF->getAllFunctions();
  }
  [[nodiscard]] f_t getFunctionImpl(llvm::StringRef Fun) const {
    return ICF->getFunction(Fun);
  }
  [[nodiscard]] bool isIndirectFunctionCallImpl(n_t Inst) const {
    return ICF->isIndirectFunctionCall(Inst);
  }
  [[nodiscard]] bool isVirtualFunctionCallImpl(n_t Inst) const {
    return ICF->isVirtualFunctionCall(Inst);
  }
  [[nodiscard]] std::vector<n_t> allNonCallStartNodesImpl() const {
    return ICF->allNonCallStartNodes();
  }
  [[nodiscard]] llvm::SmallVector<n_t> getCallsFromWithinImpl(f_t Fun) const {
    return ICF->getCallsFromWithin(Fun);
  }
  [[nodiscard]] llvm::SmallVector<n_t, 2>
  getReturnSitesOfCallAtImpl(n_t Inst) const {
    return ICF->getReturnSitesOfCallAt(Inst);
  }

  void printImpl(llvm::raw_ostream &OS) const { ICF->print(OS); }

  [[nodiscard]] const CallGraph<n_t, f_t> &getCallGraphImpl() const noexcept {
    return ICF->getCallGraph();
  }
  [[nodiscard]] size_t getNumCallSitesImpl() const noexcept {
    return ICF->getNumCallSites();
  }

  NonNullPtr<const LLVMBasedICFG> ICF;
};

} // namespace psr
