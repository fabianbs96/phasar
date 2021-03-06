/******************************************************************************
 * Copyright (c) 2018 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Module.h>
#include <llvm/PassAnalysisSupport.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>

#include <phasar/DB/ProjectIRDB.h>
#include <phasar/PhasarLLVM/ControlFlow/ICFG.h>
#include <phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IDELinearConstantAnalysis.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IDESolverTest.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IDETaintAnalysis.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IDETypeStateAnalysis.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IFDSConstAnalysis.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IFDSLinearConstantAnalysis.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IFDSSolverTest.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IFDSTaintAnalysis.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IFDSTypeAnalysis.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/IFDSUninitializedVariables.h>
#include <phasar/PhasarLLVM/IfdsIde/Problems/TypeStateDescriptions/CSTDFILEIOTypeStateDescription.h>
#include <phasar/PhasarLLVM/IfdsIde/Solver/LLVMIDESolver.h>
#include <phasar/PhasarLLVM/IfdsIde/Solver/LLVMIFDSSolver.h>
#include <phasar/PhasarLLVM/Mono/Problems/InterMonoSolverTest.h>
#include <phasar/PhasarLLVM/Mono/Problems/InterMonoTaintAnalysis.h>
#include <phasar/PhasarLLVM/Mono/Problems/IntraMonoFullConstantPropagation.h>
#include <phasar/PhasarLLVM/Mono/Problems/IntraMonoSolverTest.h>
#include <phasar/PhasarLLVM/Mono/Solver/LLVMInterMonoSolver.h>
#include <phasar/PhasarLLVM/Mono/Solver/LLVMIntraMonoSolver.h>
#include <phasar/PhasarLLVM/Pointer/LLVMTypeHierarchy.h>
#include <phasar/PhasarLLVM/Utils/DataFlowAnalysisType.h>
#include <phasar/PhasarLLVM/WPDS/Problems/WPDSLinearConstantAnalysis.h>
#include <phasar/PhasarLLVM/WPDS/Problems/WPDSSolverTest.h>
#include <phasar/PhasarPass/Options.h>
#include <phasar/PhasarPass/PhasarPass.h>
#include <phasar/Utils/EnumFlags.h>

namespace psr {

char PhasarPass::ID = 12;

PhasarPass::PhasarPass() : llvm::ModulePass(ID) {}

llvm::StringRef PhasarPass::getPassName() const { return "PhasarPass"; }

bool PhasarPass::runOnModule(llvm::Module &M) {
  // set up the IRDB
  IRDBOptions Opt = IRDBOptions::NONE;
  Opt |= IRDBOptions::WPA;
  Opt |= IRDBOptions::OWNSNOT;
  ProjectIRDB DB(Opt);
  DB.insertModule(std::unique_ptr<llvm::Module>(&M));
  DB.preprocessIR();
  // check if the requested entry points exist
  for (const std::string &EP : EntryPoints) {
    if (!DB.getFunction(EP)) {
      llvm::report_fatal_error("psr error: entry point does not exist '" + EP +
                               "'");
    }
  }
  // set up the call-graph algorithm to be used
  CallGraphAnalysisType CGTy =
      StringToCallGraphAnalysisType.at(CallGraphAnalysis);
  LLVMTypeHierarchy H(DB);
  LLVMBasedCFG CFG;
  LLVMBasedICFG I(H, DB, CGTy, EntryPoints);
  if (DataFlowAnalysis == "ifds-solvertest") {
    IFDSSolverTest ifdstest(I, H, DB, EntryPoints);
    LLVMIFDSSolver<const llvm::Value *, LLVMBasedICFG &> llvmifdstestsolver(
        ifdstest, DumpResults);
    llvmifdstestsolver.solve();
  } else if (DataFlowAnalysis == "ide-solvertest") {
    IDESolverTest idetest(I, H, DB, EntryPoints);
    LLVMIDESolver<const llvm::Value *, const llvm::Value *, LLVMBasedICFG &>
        llvmidetestsolver(idetest, DumpResults);
    llvmidetestsolver.solve();
  } else if (DataFlowAnalysis == "intra-mono-solvertest") {
    const llvm::Function *F = DB.getFunction(EntryPoints.front());
    IntraMonoSolverTest intra(CFG, F);
    LLVMIntraMonoSolver<const llvm::Value *, LLVMBasedCFG &> solver(
        intra, DumpResults);
    solver.solve();
  } else if (DataFlowAnalysis == "inter-mono-solvertest") {
    const llvm::Function *F = DB.getFunction(EntryPoints.front());
    InterMonoSolverTest inter(I, EntryPoints);
    LLVMInterMonoSolver<const llvm::Value *, LLVMBasedICFG &, 0> solver(inter,
                                                                        true);

    solver.solve();
  } else if (DataFlowAnalysis == "ifds-const") {
    IFDSConstAnalysis constproblem(I, H, DB, DB.getAllMemoryLocations(),
                                   EntryPoints);
    LLVMIFDSSolver<const llvm::Value *, LLVMBasedICFG &> llvmconstsolver(
        constproblem, DumpResults);
    llvmconstsolver.solve();
  } else if (DataFlowAnalysis == "ifds-lca") {
    IFDSLinearConstantAnalysis lcaproblem(I, H, DB, EntryPoints);
    LLVMIFDSSolver<LCAPair, LLVMBasedICFG &> llvmlcasolver(lcaproblem,
                                                           DumpResults);
    llvmlcasolver.solve();
  } else if (DataFlowAnalysis == "ifds-taint") {
    TaintSensitiveFunctions TSF;
    IFDSTaintAnalysis TaintAnalysisProblem(I, H, DB, TSF, EntryPoints);
    LLVMIFDSSolver<const llvm::Value *, LLVMBasedICFG &> LLVMTaintSolver(
        TaintAnalysisProblem, DumpResults);
    LLVMTaintSolver.solve();
  } else if (DataFlowAnalysis == "ifds-type") {
    IFDSTypeAnalysis typeanalysisproblem(I, H, DB, EntryPoints);
    LLVMIFDSSolver<const llvm::Value *, LLVMBasedICFG &> llvmtypesolver(
        typeanalysisproblem, DumpResults);
    llvmtypesolver.solve();
  } else if (DataFlowAnalysis == "ifds-uninit") {
    IFDSUnitializedVariables uninitializedvarproblem(I, H, DB, EntryPoints);
    LLVMIFDSSolver<const llvm::Value *, LLVMBasedICFG &> llvmunivsolver(
        uninitializedvarproblem, DumpResults);
    llvmunivsolver.solve();
  } else if (DataFlowAnalysis == "ide-lca") {
    IDELinearConstantAnalysis lcaproblem(I, H, DB, EntryPoints);
    LLVMIDESolver<const llvm::Value *, int64_t, LLVMBasedICFG &> llvmlcasolver(
        lcaproblem, DumpResults);
    llvmlcasolver.solve();
  } else if (DataFlowAnalysis == "ide-taint") {
    IDETaintAnalysis taintanalysisproblem(I, H, DB, EntryPoints);
    LLVMIDESolver<const llvm::Value *, const llvm::Value *, LLVMBasedICFG &>
        llvmtaintsolver(taintanalysisproblem, DumpResults);
    llvmtaintsolver.solve();
  } else if (DataFlowAnalysis == "ide-typestate") {
    CSTDFILEIOTypeStateDescription fileIODesc;
    IDETypeStateAnalysis typestateproblem(I, H, DB, fileIODesc, EntryPoints);
    LLVMIDESolver<const llvm::Value *, int, LLVMBasedICFG &>
        llvmtypestatesolver(typestateproblem, DumpResults);
    llvmtypestatesolver.solve();
  } else if (DataFlowAnalysis == "intra-mono-fullconstantpropagation") {
    // todo
  } else if (DataFlowAnalysis == "inter-mono-taint") {
    // todo
  } else if (DataFlowAnalysis == "plugin") {
    // todo
  } else if (DataFlowAnalysis == "none") {
    // do nothing
  }
  return false;
}

bool PhasarPass::doInitialization(llvm::Module &M) {
  llvm::outs() << "PhasarPass::doInitialization()\n";
  initializeLogger(InitLogger);
  // check the user's parameters
  if (EntryPoints.empty()) {
    llvm::report_fatal_error("psr error: no entry points provided");
  }
  if (!StringToCallGraphAnalysisType.count(CallGraphAnalysis)) {
    llvm::report_fatal_error("psr error: call-graph analysis does not exist");
  }
  if (!StringToDataFlowAnalysisType.count(DataFlowAnalysis)) {
    llvm::report_fatal_error("psr error: data-flow analysis does not exist");
  }
  return false;
}

bool PhasarPass::doFinalization(llvm::Module &M) {
  llvm::outs() << "PhasarPass::doFinalization()\n";
  return false;
}

void PhasarPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {}

void PhasarPass::releaseMemory() {}

void PhasarPass::print(llvm::raw_ostream &O, const llvm::Module *M) const {
  O << "I am a PhasarPass Analysis Result ;-)\n";
}

static llvm::RegisterPass<PhasarPass> phasar("phasar", "PhASAR Pass",
                                             false /* Only looks at CFG */,
                                             false /* Analysis Pass */);

} // namespace psr
