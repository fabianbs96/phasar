#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

namespace llvm {
class Value;
class Instruction;
} // namespace llvm

namespace psr::detail {

/// \file
/// Alias/points-to-agnostic helpers shared by SparseLLVMControlFlow
/// and SparsePointsToControlFlow

[[nodiscard]] bool isSparseNoopIntrinsic(const llvm::Instruction *Inst);

[[nodiscard]] bool isSparseExitInst(const llvm::Instruction *Inst);

/// \brief Whether Val is a local alloca whose address is never taken, i.e.
/// no other pointer can ever refer to the same memory.
[[nodiscard]] bool isNonAddressTakenVariable(const llvm::Value *Val);

} // namespace psr::detail
