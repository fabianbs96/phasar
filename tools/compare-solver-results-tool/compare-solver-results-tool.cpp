/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include "phasar.h"

#include <filesystem>
#include <string>

using namespace psr;

int main(int Argc, const char **Argv) {
  using namespace std::string_literals;

  if (Argc < 2 || !std::filesystem::exists(Argv[1]) ||
      std::filesystem::is_directory(Argv[1])) {
    llvm::errs() << "compare-solver-results-tool\n"
                    "A small PhASAR-based program that runs two solvers and "
                    "compares if the results are the same.\n\n"
                    "Usage: compare-solver-results-tool <LLVM IR file>\n";
    return 1;
  }

  std::vector EntryPoints = {"main"s};

  HelperAnalyses HA(Argv[1], EntryPoints);
  if (!HA.getProjectIRDB().isValid()) {
    return 1;
  }

  if (HA.getProjectIRDB().getFunctionDefinition("main")) {
    // IDE template parametrization test
    llvm::outs() << "Testing IDE:\n";
    auto M = createAnalysisProblem<IDELinearConstantAnalysis>(HA, EntryPoints);
    // Alternative way of solving an IFDS/IDEProblem:
    auto IDEResults = solveIDEProblem(M, HA.getICFG());
    auto ParallelIDEResults = solveIDEProblemPll(M, HA.getICFG());

    if (checkSREquality(IDEResults, ParallelIDEResults)) {
      llvm::outs() << "\nSuccess! Results are equal!\n";
    } else {
      llvm::outs() << "\nFailure! Results are not equal!\n";
    }
  } else {
    llvm::errs() << "error: file does not contain a 'main' function!\n";
  }
  return 0;
}
