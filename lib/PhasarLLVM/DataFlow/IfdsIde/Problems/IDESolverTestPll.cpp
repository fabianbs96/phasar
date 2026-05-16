/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDESolverTestPll.h"

#include "phasar/DataFlow/IfdsIde/EdgeFunctionUtils.h"
#include "phasar/DataFlow/IfdsIde/EdgeFunctions.h"
#include "phasar/DataFlow/IfdsIde/FlowFunctions.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/LLVMZeroValue.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

#include <utility>

namespace psr {

IDESolverTestPll::IDESolverTestPll(const LLVMProjectIRDB *IRDB,
                                   std::vector<std::string> EntryPoints)
    : IDETabulationProblem(IRDB, std::move(EntryPoints), createZeroValue()) {}

// start formulating our analysis by specifying the parts required for IFDS

IDESolverTestPll::FlowFunctionPtrType
IDESolverTestPll::getNormalFlowFunction(IDESolverTestPll::n_t /*Curr*/,
                                        IDESolverTestPll::n_t /*Succ*/) {
  return identityFlow();
}

IDESolverTestPll::FlowFunctionPtrType
IDESolverTestPll::getCallFlowFunction(IDESolverTestPll::n_t /*CallSite*/,
                                      IDESolverTestPll::f_t /*DestFun*/) {
  return identityFlow();
}

IDESolverTestPll::FlowFunctionPtrType IDESolverTestPll::getRetFlowFunction(
    IDESolverTestPll::n_t /*CallSite*/, IDESolverTestPll::f_t /*CalleeFun*/,
    IDESolverTestPll::n_t /*ExitStmt*/, IDESolverTestPll::n_t /*RetSite*/) {
  return identityFlow();
}

IDESolverTestPll::FlowFunctionPtrType
IDESolverTestPll::getCallToRetFlowFunction(IDESolverTestPll::n_t /*CallSite*/,
                                           IDESolverTestPll::n_t /*RetSite*/,
                                           llvm::ArrayRef<f_t> /*Callees*/) {
  return identityFlow();
}

IDESolverTestPll::FlowFunctionPtrType
IDESolverTestPll::getSummaryFlowFunction(IDESolverTestPll::n_t /*CallSite*/,
                                         IDESolverTestPll::f_t /*DestFun*/) {
  return nullptr;
}

InitialSeeds<IDESolverTestPll::n_t, IDESolverTestPll::d_t,
             IDESolverTestPll::l_t>
IDESolverTestPll::initialSeeds() {
  PHASAR_LOG_LEVEL(DEBUG, "IDESolverTestPll::initialSeeds()");
  return createDefaultSeeds();
}

IDESolverTestPll::d_t IDESolverTestPll::createZeroValue() const {
  PHASAR_LOG_LEVEL(DEBUG, "IDESolverTestPll::createZeroValue()");
  // create a special value to represent the zero value!
  return LLVMZeroValue::getInstance();
}

bool IDESolverTestPll::isZeroValue(IDESolverTestPll::d_t Fact) const noexcept {
  return LLVMZeroValue::isLLVMZeroValue(Fact);
}

// in addition provide specifications for the IDE parts

EdgeFunction<IDESolverTestPll::l_t> IDESolverTestPll::getNormalEdgeFunction(
    IDESolverTestPll::n_t /*Curr*/, IDESolverTestPll::d_t /*CurrNode*/,
    IDESolverTestPll::n_t /*Succ*/, IDESolverTestPll::d_t /*SuccNode*/) {
  return EdgeIdentity<IDESolverTestPll::l_t>{};
}

EdgeFunction<IDESolverTestPll::l_t> IDESolverTestPll::getCallEdgeFunction(
    IDESolverTestPll::n_t /*CallSite*/, IDESolverTestPll::d_t /*SrcNode*/,
    IDESolverTestPll::f_t /*DestinationFunction*/,
    IDESolverTestPll::d_t /*DestNode*/) {
  return EdgeIdentity<IDESolverTestPll::l_t>{};
}

EdgeFunction<IDESolverTestPll::l_t> IDESolverTestPll::getReturnEdgeFunction(
    IDESolverTestPll::n_t /*CallSite*/,
    IDESolverTestPll::f_t /*CalleeFunction*/,
    IDESolverTestPll::n_t /*ExitStmt*/, IDESolverTestPll::d_t /*ExitNode*/,
    IDESolverTestPll::n_t /*RetSite*/, IDESolverTestPll::d_t /*RetNode*/) {
  return EdgeIdentity<IDESolverTestPll::l_t>{};
}

EdgeFunction<IDESolverTestPll::l_t> IDESolverTestPll::getCallToRetEdgeFunction(
    IDESolverTestPll::n_t /*CallSite*/, IDESolverTestPll::d_t /*CallNode*/,
    IDESolverTestPll::n_t /*RetSite*/, IDESolverTestPll::d_t /*RetSiteNode*/,
    llvm::ArrayRef<f_t> /*Callees*/) {
  return EdgeIdentity<IDESolverTestPll::l_t>{};
}

EdgeFunction<IDESolverTestPll::l_t> IDESolverTestPll::getSummaryEdgeFunction(
    IDESolverTestPll::n_t /*CallSite*/, IDESolverTestPll::d_t /*CallNode*/,
    IDESolverTestPll::n_t /*RetSite*/, IDESolverTestPll::d_t /*RetSiteNode*/) {
  return EdgeIdentity<IDESolverTestPll::l_t>{};
}

IDESolverTestPll::l_t IDESolverTestPll::topElement() {
  PHASAR_LOG_LEVEL(DEBUG, "IDESolverTestPll::topElement()");
  return nullptr;
}

IDESolverTestPll::l_t IDESolverTestPll::bottomElement() {
  PHASAR_LOG_LEVEL(DEBUG, "IDESolverTestPll::bottomElement()");
  return nullptr;
}

IDESolverTestPll::l_t IDESolverTestPll::join(IDESolverTestPll::l_t /*Lhs*/,
                                             IDESolverTestPll::l_t /*Rhs*/) {
  PHASAR_LOG_LEVEL(DEBUG, "IDESolverTestPll::join()");
  return nullptr;
}

EdgeFunction<IDESolverTestPll::l_t> IDESolverTestPll::allTopFunction() {
  PHASAR_LOG_LEVEL(DEBUG, "IDESolverTestPll::allTopFunction()");
  return AllTop<l_t>{nullptr};
}

} // namespace psr
