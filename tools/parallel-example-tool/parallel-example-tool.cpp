/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IDELinearConstantAnalysis.h"

#include "phasar.h"

#include <filesystem>
#include <string>

using namespace psr;

int main(int Argc, const char **Argv) {
  using namespace std::string_literals;

  if (Argc < 2 || !std::filesystem::exists(Argv[1]) ||
      std::filesystem::is_directory(Argv[1])) {
    llvm::errs() << "myphasartool\n"
                    "A small PhASAR-based example program\n\n"
                    "Usage: myphasartool <LLVM IR file>\n";
    return 1;
  }

  // Logger::initializeStderrLogger(SeverityLevel::INFO);
  // Logger::initializeStderrLogger(SeverityLevel::DEBUG);
  // Logger::initializeStderrLogger(SeverityLevel::ERROR);
  // Logger::initializeStderrLogger(SeverityLevel::CRITICAL);
  // Logger::initializeStderrLogger(SeverityLevel::INVALID);
  // Logger::initializeStderrLogger(SeverityLevel::WARNING);
  // Logger::initializeStderrLogger(SeverityLevel::DFADEBUG);

  std::vector EntryPoints = {"main"s};

  HelperAnalyses HA(Argv[1], EntryPoints);
  if (!HA.getProjectIRDB().isValid()) {
    return 1;
  }

  if (HA.getProjectIRDB().getFunctionDefinition("main")) {
    auto Problem =
        createAnalysisProblem<IDELinearConstantAnalysis>(HA, EntryPoints);

    llvm::outs() << "Testing ParallelizedIDESolver:\n";
    ParallelizedIDESolver Solver(Problem, &HA.getICFG());
    Solver.solve();
    auto PIDEResults = Solver.getSolverResults();
    PIDEResults.dumpResults(HA.getICFG());
  } else {
    llvm::errs() << "error: file does not contain a 'main' function!\n";
  }
  return 0;
}
