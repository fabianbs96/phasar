/******************************************************************************
 * Copyright (c) 2021 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_TAINT_CONFIG_TAINT_CONFIG_UTILITIES_H
#define PHASAR_PHASARLLVM_TAINT_CONFIG_TAINT_CONFIG_UTILITIES_H

#include <algorithm>
#include <iterator>
#include <type_traits>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

#include "phasar/PhasarLLVM/TaintConfig/TaintConfig.h"
#include "phasar/Utils/LLVMShorthands.h"

namespace psr {
template <typename ContainerTy,
          typename = std::enable_if_t<std::is_same_v<
              typename ContainerTy::value_type, const llvm::Value *>>>
void collectGeneratedFacts(ContainerTy &Dest, const TaintConfig &Config,
                           const llvm::CallBase *CB,
                           const llvm::Function *Callee) {
  Config.forAllGeneratedValuesAt(
      CB, CB->getNextNode(), Callee,
      [&Dest](const llvm::Value *V) { Dest.insert(V); });
}

template <typename ContainerTy, typename Pred,
          typename = std::enable_if_t<std::is_same_v<
              typename ContainerTy::value_type, const llvm::Value *>>>
void collectLeakedFacts(ContainerTy &Dest, const TaintConfig &Config,
                        const llvm::CallBase *CB, const llvm::Function *Callee,
                        Pred &&LeakIf) {

  Config.forAllLeakCandidatesAt(CB, CB->getNextNode(), Callee,
                                [&Dest, &LeakIf](const llvm::Value *V) {
                                  if (LeakIf(V)) {
                                    Dest.insert(V);
                                  }
                                });
}

template <typename ContainerTy>
inline void collectLeakedFacts(ContainerTy &Dest, const TaintConfig &Config,
                               const llvm::CallBase *CB,
                               const llvm::Function *Callee) {
  collectLeakedFacts(Dest, Config, CB, Callee,
                     [](const llvm::Value *V) { return true; });
}

template <typename ContainerTy,
          typename = std::enable_if_t<std::is_same_v<
              typename ContainerTy::value_type, const llvm::Value *>>>
void collectSanitizedFacts(ContainerTy &Dest, const TaintConfig &Config,
                           const llvm::CallBase *CB,
                           const llvm::Function *Callee) {
  Config.forAllSanitizedValuesAt(
      CB, CB->getNextNode(), Callee,
      [&Dest](const llvm::Value *V) { Dest.insert(V); });
}
} // namespace psr

#endif // PHASAR_PHASARLLVM_TAINT_CONFIG_TAINT_CONFIG_UTILITIES_H