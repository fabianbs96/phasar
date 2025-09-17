/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * RTAResolver.cpp
 *
 *  Created on: 20.07.2018
 *      Author: nicolas bellec
 */

#include "phasar/PhasarLLVM/ControlFlow/Resolver/RTAResolver.h"

#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/PhasarLLVM/Utils/AllocatedTypes.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Logger.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

using namespace psr;

RTAResolver::RTAResolver(NonNullPtr<const LLVMProjectIRDB> IRDB,
                         NonNullPtr<const LLVMVFTableProvider> VTP,
                         const DIBasedTypeHierarchy *TH)
    : CHAResolver(IRDB, VTP, TH) {
  resolveAllocatedCompositeTypes();
}

bool RTAResolver::resolve(const llvm::CallBase *Call,
                          FunctionSetTy &PossibleTargets) {
  PHASAR_LOG_LEVEL(DEBUG, "Call virtual function: " << llvmIRToString(Call));

  auto RetrievedVtableIndex = getVFTIndex(Call);
  if (!RetrievedVtableIndex.has_value()) {
    return false;
  }

  auto VtableIndex = RetrievedVtableIndex.value();

  PHASAR_LOG_LEVEL(DEBUG, "Virtual function table entry is: " << VtableIndex);

  const auto *ReceiverType = getReceiverType(Call);

  // also insert all possible subtypes vtable entries
  auto ReachableTypes = TH->getSubTypes(ReceiverType);

  // also insert all possible subtypes vtable entries
  auto EndIt = ReachableTypes.end();
  for (const auto *PossibleType : AllocatedCompositeTypes) {
    if (ReachableTypes.find(PossibleType) != EndIt) {

      const auto *Target = getNonPureVirtualVFTEntry(PossibleType, VtableIndex,
                                                     Call, ReceiverType);
      if (Target && psr::isConsistentCall(Call, Target)) {
        PossibleTargets.insert(Target);
      }
    }
  }

  return !PossibleTargets.empty();
}

void RTAResolver::resolveVirtualCall(FunctionSetTy &PossibleTargets,
                                     const llvm::CallBase *CallSite) {

  if (!resolve(CallSite, PossibleTargets)) {
    CHAResolver::resolve(CallSite, PossibleTargets);
  }
}

std::string RTAResolver::str() const { return "RTA"; }

void RTAResolver::resolveAllocatedCompositeTypes() {
  if (!AllocatedCompositeTypes.empty()) {
    return;
  }

  AllocatedCompositeTypes = collectAllocatedTypes(*IRDB->getModule());
}
