/******************************************************************************
 * Copyright (c) 2020 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "llvm/IR/Instruction.h"

#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/ExtendedTaintAnalysis/ComposeEdgeFunction.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/ExtendedTaintAnalysis/GenEdgeFunction.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/ExtendedTaintAnalysis/JoinConstEdgeFunction.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/ExtendedTaintAnalysis/JoinEdgeFunction.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/ExtendedTaintAnalysis/KillIfSanitizedEdgeFunction.h"
#include "phasar/PhasarLLVM/Utils/BasicBlockOrdering.h"
#include "phasar/Utils/LLVMShorthands.h"

namespace psr::XTaint {
KillIfSanitizedEdgeFunction::KillIfSanitizedEdgeFunction(
    BasicBlockOrdering &BBO, const llvm::Instruction *Load)
    : EdgeFunctionBase(Kind::KillIfSani, BBO), Load(Load) {}

KillIfSanitizedEdgeFunction::l_t
KillIfSanitizedEdgeFunction::computeTarget(l_t Source) {

  if (const auto *Sani = Source.getSanitizer()) {
    if (!Load) {
      return Sanitized{};
    }
    if (Sani->getFunction() == Load->getFunction() &&
        BBO.mustComeBefore(Sani, Load)) {
      return Sanitized{};
    }

    return nullptr;
  }

  return Source;
}

KillIfSanitizedEdgeFunction::EdgeFunctionPtrType
KillIfSanitizedEdgeFunction::composeWith(EdgeFunctionPtrType SecondFunction) {
  if (this == &*SecondFunction || equal_to(SecondFunction)) {
    return shared_from_this();
  }
  return EdgeFunctionBase::composeWith(std::move(SecondFunction));
}

bool KillIfSanitizedEdgeFunction::equal_to(
    EdgeFunctionPtrType OtherFunction) const {
  if (auto *OtherKill =
          dynamic_cast<KillIfSanitizedEdgeFunction *>(&*OtherFunction)) {
    return Load == OtherKill->Load; // Assume, the Analysis to be the same
  }
  return false;
}

llvm::hash_code KillIfSanitizedEdgeFunction::getHashCode() const {
  return llvm::hash_combine(Load);
}

void KillIfSanitizedEdgeFunction::print(
    std::ostream &OS, [[maybe_unused]] bool IsForDebug) const {
  OS << "KillIfSani[" << this << "]";
}

} // namespace psr::XTaint
