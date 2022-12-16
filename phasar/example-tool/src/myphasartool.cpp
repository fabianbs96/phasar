/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <filesystem>
#include <fstream>

#include "phasar/DB/ProjectIRDB.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/CallGraphAnalysisType.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/IFDSSolverTest.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Solver/IDESolver.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Solver/IFDSSolver.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToSet.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h"
#include "phasar/Utils/Logger.h"

namespace llvm {
class Value;
} // namespace llvm

struct Elapsed {
  int64_t Nanos;
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, Elapsed E) {
  if (E.Nanos > 60'000'000'000LL) {
    OS << llvm::format("%.2f", double(E.Nanos) / 60'000'000'000LL) << "m";
  } else if (E.Nanos > 1'000'000'000LL) {
    OS << llvm::format("%.2f", double(E.Nanos) / 1'000'000'000LL) << "s";
  } else if (E.Nanos > 1'000'000LL) {
    OS << llvm::format("%.2f", float(E.Nanos) / 1'000'000) << "ms";
  } else if (E.Nanos > 1'000LL) {
    OS << llvm::format("%.2f", float(E.Nanos) / 1'000) << "us";
  } else {
    OS << E.Nanos << "ns";
  }
  return OS;
}

using namespace psr;

int main(int Argc, const char **Argv) {
  if (Argc < 2 || !std::filesystem::exists(Argv[1]) ||
      std::filesystem::is_directory(Argv[1])) {
    llvm::errs() << "myphasartool\n"
                    "A small PhASAR-based example program\n\n"
                    "Usage: myphasartool <LLVM IR file>\n";
    return 1;
  }
  ProjectIRDB DB({Argv[1]});
  if (const auto *F = DB.getFunctionDefinition("main")) {
    LLVMTypeHierarchy H(DB);

    LLVMPointsToSet P(DB);
    // print points-to information
    P.print();
    LLVMBasedICFG I(&DB, CallGraphAnalysisType::OTF, {"main"}, &H, &P);
    // print inter-procedural control-flow graph
    I.print();
    // IFDS template parametrization test
    llvm::outs() << "Testing IFDS:\n";
    IFDSSolverTest L(&DB, &H, &I, &P, {"main"});
    IFDSSolver S(L);
    S.solve();
    S.dumpResults();
    // IDE template parametrization test
    llvm::outs() << "Testing IDE:\n";
    IDELinearConstantAnalysis M(&DB, &H, &I, &P, {"main"});
    IDESolver T(M);
    T.solve();
    T.dumpResults();
  } else {
    llvm::errs() << "error: file does not contain a 'main' function!\n";
  }
  return 0;
}
