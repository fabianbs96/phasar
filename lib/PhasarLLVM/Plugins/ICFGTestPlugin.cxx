/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <utility>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"

#include "phasar/DB/ProjectIRDB.h"

#include "ICFGTestPlugin.h"

using namespace std;
using namespace psr;

namespace psr {

__attribute__((constructor)) void init() {
  llvm::outs() << "init - ICFGTestPlugin\n";
  ICFGPluginFactory["icfg_testplugin"] = &makeICFGTestPlugin;
}

__attribute__((destructor)) void fini() {
  llvm::outs() << "fini - ICFGTestPlugin\n";
}

unique_ptr<ICFGPlugin> makeICFGTestPlugin(ProjectIRDB &IRDB,
                                          const vector<string> &EntryPoints) {
  return unique_ptr<ICFGPlugin>(new ICFGTestPlugin(IRDB, EntryPoints));
}

ICFGTestPlugin::ICFGTestPlugin(ProjectIRDB &IRDB,
                               const vector<string> &EntryPoints)
    : ICFGPlugin(IRDB, EntryPoints) {}

ICFGTestPlugin::f_t
ICFGTestPlugin::getFunctionOf(ICFGTestPlugin::n_t /*Inst*/) const {
  return nullptr;
}

std::vector<ICFGTestPlugin::n_t>
ICFGTestPlugin::getPredsOf(ICFGTestPlugin::n_t /*Inst*/) const {
  return {};
}

std::vector<ICFGTestPlugin::n_t>
ICFGTestPlugin::getSuccsOf(ICFGTestPlugin::n_t /*Inst*/) const {
  return {};
}

std::vector<std::pair<ICFGTestPlugin::n_t, ICFGTestPlugin::n_t>>
ICFGTestPlugin::getAllControlFlowEdges(ICFGTestPlugin::f_t /*Fun*/) const {
  return {};
}

std::vector<ICFGTestPlugin::n_t>
ICFGTestPlugin::getAllInstructionsOf(ICFGTestPlugin::f_t /*Fun*/) const {
  return {};
}

std::set<ICFGTestPlugin::n_t>
ICFGTestPlugin::getStartPointsOf(ICFGTestPlugin::f_t /*Fun*/) const {
  return {};
}

std::set<ICFGTestPlugin::n_t>
ICFGTestPlugin::getExitPointsOf(ICFGTestPlugin::f_t /*Fun*/) const {
  return {};
}

void ICFGTestPlugin::collectGlobalCtors() {}

void ICFGTestPlugin::collectGlobalDtors() {}

void ICFGTestPlugin::collectGlobalInitializers() {}

void ICFGTestPlugin::collectRegisteredDtors() {}

bool ICFGTestPlugin::isExitInst(ICFGTestPlugin::n_t /*Inst*/) const {
  return false;
}

bool ICFGTestPlugin::isStartPoint(ICFGTestPlugin::n_t /*Inst*/) const {
  return false;
}

bool ICFGTestPlugin::isFieldLoad(ICFGTestPlugin::n_t /*Inst*/) const {
  return false;
}

bool ICFGTestPlugin::isFieldStore(ICFGTestPlugin::n_t /*Inst*/) const {
  return false;
}

bool ICFGTestPlugin::isFallThroughSuccessor(
    ICFGTestPlugin::n_t /*Inst*/, ICFGTestPlugin::n_t /*Succ*/) const {
  return false;
}

bool ICFGTestPlugin::isBranchTarget(ICFGTestPlugin::n_t /*Inst*/,
                                    ICFGTestPlugin::n_t /*Succ*/) const {
  return false;
}

bool ICFGTestPlugin::isHeapAllocatingFunction(
    ICFGTestPlugin::f_t /*Fun*/) const {
  return false;
}

bool ICFGTestPlugin::isSpecialMemberFunction(
    ICFGTestPlugin::f_t /*Fun*/) const {
  return false;
}

SpecialMemberFunctionType ICFGTestPlugin::getSpecialMemberFunctionType(
    ICFGTestPlugin::f_t /*Fun*/) const {
  return SpecialMemberFunctionType::None;
}

std::string ICFGTestPlugin::getStatementId(ICFGTestPlugin::n_t /*Inst*/) const {
  return "";
}

std::string ICFGTestPlugin::getFunctionName(ICFGTestPlugin::f_t /*Fun*/) const {
  return "";
}

std::string
ICFGTestPlugin::getDemangledFunctionName(ICFGTestPlugin::f_t /*Fun*/) const {
  return "";
}

void ICFGTestPlugin::print(ICFGTestPlugin::f_t /*F*/,
                           llvm::raw_ostream & /*OS*/) const {}

nlohmann::json ICFGTestPlugin::getAsJson(ICFGTestPlugin::f_t /*F*/) const {
  return "";
}

// ICFG parts

std::set<ICFGTestPlugin::f_t> ICFGTestPlugin::getAllFunctions() const {
  return {};
}

ICFGTestPlugin::f_t
ICFGTestPlugin::getFunction(const std::string & /*Fun*/) const {
  return nullptr;
}

bool ICFGTestPlugin::isCallSite(ICFGTestPlugin::n_t /*Inst*/) const {
  return false;
}

bool ICFGTestPlugin::isIndirectFunctionCall(
    ICFGTestPlugin::n_t /*Inst*/) const {
  return false;
}

bool ICFGTestPlugin::isVirtualFunctionCall(ICFGTestPlugin::n_t /*Inst*/) const {
  return false;
}

std::set<ICFGTestPlugin::n_t> ICFGTestPlugin::allNonCallStartNodes() const {
  return {};
}

std::set<ICFGTestPlugin::f_t>
ICFGTestPlugin::getCalleesOfCallAt(ICFGTestPlugin::n_t /*Inst*/) const {
  return {};
}

std::set<ICFGTestPlugin::n_t>
ICFGTestPlugin::getCallersOf(ICFGTestPlugin::f_t /*Fun*/) const {
  return {};
}

std::set<ICFGTestPlugin::n_t>
ICFGTestPlugin::getCallsFromWithin(ICFGTestPlugin::f_t /*Fun*/) const {
  return {};
}

std::set<ICFGTestPlugin::n_t>
ICFGTestPlugin::getReturnSitesOfCallAt(ICFGTestPlugin::n_t /*Inst*/) const {
  return {};
}

void ICFGTestPlugin::print(llvm::raw_ostream & /*OS*/) const {}

nlohmann::json ICFGTestPlugin::getAsJson() const { return ""_json; }

} // namespace psr
