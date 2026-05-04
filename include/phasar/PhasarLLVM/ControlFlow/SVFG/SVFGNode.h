#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/Utils/StrongTypeDef.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include <cstdint>

// enum class SVFGNodeId : uint32_t {} + llvm::DenseMapInfo<psr::SVFGNodeId>
PHASAR_STRONG_TYPEDEF(psr, uint32_t, SVFGNodeId)

namespace psr {

enum class SVFGNodeKind : uint8_t {
  Addr,        // alloca or global (base address definition)
  Copy,        // SSA copy: phi, select, cast, arithmetic, etc.
  Gep,         // getelementptr (field/index address computation)
  Store,       // store instruction (indirect write to memory)
  Load,        // load instruction (indirect read from memory)
  ActualParam, // actual argument at a call site (call boundary)
  FormalParam, // formal parameter at function entry
  ActualRet,   // return value received at a call site
  FormalRet,   // return instruction in a callee (exit boundary)
};

[[nodiscard]] llvm::StringRef svfgNodeKindName(SVFGNodeKind K) noexcept;

struct SVFGNode {
  SVFGNodeKind Kind;
  // Using Value* instead of Instruction* because llvm::Argument is a Value
  // but not an Instruction.
  const llvm::Value *IRValue = nullptr;
  // Non-null for ActualParam/ActualRet nodes.
  const llvm::CallBase *CallSite = nullptr;
  // Containing function (nullptr for globals).
  const llvm::Function *Function = nullptr;
};

} // namespace psr
