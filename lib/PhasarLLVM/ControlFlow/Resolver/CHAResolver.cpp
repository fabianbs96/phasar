/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * CHAResolver.cpp
 *
 *  Created on: 20.07.2018
 *      Author: nicolas bellec
 */

#include "phasar/PhasarLLVM/ControlFlow/Resolver/CHAResolver.h"

#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Logger.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

#include <memory>

using namespace std;
using namespace psr;

CHAResolver::CHAResolver(const LLVMProjectIRDB *IRDB,
                         const LLVMVFTableProvider *VTP,
                         const DIBasedTypeHierarchy *TH)
    : Resolver(IRDB, VTP), TH(TH) {
  if (!TH) {
    this->TH = std::make_unique<DIBasedTypeHierarchy>(*IRDB);
  }
}

CHAResolver::~CHAResolver() = default;

bool CHAResolver::resolve(const llvm::CallBase *Call,
                          FunctionSetTy &PossibleTargets) {

  auto RetrievedVtableIndex = getVFTIndex(Call);
  if (!RetrievedVtableIndex.has_value()) {
    return false;
  }

  auto VtableIndex = RetrievedVtableIndex.value();

  PHASAR_LOG_LEVEL(DEBUG, "Virtual function table entry is: " << VtableIndex);

  const auto *ReceiverTy = getReceiverType(Call);

  // also insert all possible subtypes vtable entries
  auto FallbackTys = TH->getSubTypes(ReceiverTy);

  for (const auto &FallbackTy : FallbackTys) {
    const auto *Target =
        getNonPureVirtualVFTEntry(FallbackTy, VtableIndex, Call);
    if (Target) {
      PossibleTargets.insert(Target);
    }
  }
  return !PossibleTargets.empty();
}

auto CHAResolver::resolveVirtualCall(const llvm::CallBase *CallSite)
    -> FunctionSetTy {
  PHASAR_LOG_LEVEL(DEBUG, "Call virtual function: ");

  FunctionSetTy PossibleCallees;
  resolve(CallSite, PossibleCallees);
  return PossibleCallees;
}

std::string CHAResolver::str() const { return "CHA"; }
