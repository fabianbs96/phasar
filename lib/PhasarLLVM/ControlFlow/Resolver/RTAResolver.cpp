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
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"

using namespace std;
using namespace psr;

RTAResolver::RTAResolver(const LLVMProjectIRDB *IRDB,
                         const LLVMVFTableProvider *VTP,
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
      const auto *Target =
          getNonPureVirtualVFTEntry(PossibleType, VtableIndex, Call);
      if (Target) {
        PossibleTargets.insert(Target);
      }
    }
  }

  return !PossibleTargets.empty();
}

auto RTAResolver::resolveVirtualCall(const llvm::CallBase *CallSite)
    -> FunctionSetTy {

  FunctionSetTy PossibleCallTargets;

  if (!resolve(CallSite, PossibleCallTargets)) {
    CHAResolver::resolve(CallSite, PossibleCallTargets);
  }

  return PossibleCallTargets;
}

std::string RTAResolver::str() const { return "RTA"; }

/// More or less copied from GeneralStatisticsAnalysis
void RTAResolver::resolveAllocatedCompositeTypes() {
  if (!AllocatedCompositeTypes.empty()) {
    return;
  }

  llvm::DebugInfoFinder DIF;
  DIF.processModule(*IRDB->getModule());

  for (const auto *Ty : DIF.types()) {
    if (const auto *CompTy = llvm::dyn_cast<llvm::DICompositeType>(Ty)) {
      AllocatedCompositeTypes.push_back(CompTy);
    }
  }
}
