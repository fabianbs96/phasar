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

static bool isInt(const char *CCStr) {
  if (!CCStr) {
    return true;
  }

  std::string Str(CCStr);

  for (const auto CurrChar : Str) {
    if (!isdigit(CurrChar)) {
      return false;
    }
  }

  return true;
}

int main(int Argc, const char **Argv) {
  using namespace std::string_literals;

  if (Argc < 2 || !std::filesystem::exists(Argv[1]) ||
      std::filesystem::is_directory(Argv[1]) ||
      (Argc == 3 && !isInt(Argv[2]))) {
    llvm::errs()
        << "parallel-example-tool\n"
           "A small PhASAR-based parallel IDE solver implementation.\n\n"
           "Usage: myphasartool <LLVM IR file> <Number of threads>\n";
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

  HelperAnalyses HA(
      Argv[1], EntryPoints,
      {.PTATy = AliasAnalysisType::UnionFind, .AllowLazyPTS = false});
  if (!HA.getProjectIRDB().isValid()) {
    return 1;
  }

  if (HA.getProjectIRDB().getFunctionDefinition("main")) {
    llvm::outs() << "Testing ParallelizedIDESolver:\n";

    auto Problem =
        createAnalysisProblem<IDELinearConstantAnalysis>(HA, EntryPoints);

    if (Argc == 3) {
      auto IDEResults = solveIDEProblemPll(Problem, HA.getICFG(),
                                           std::stoi(std::string(Argv[2])));
    } else {
      auto IDEResults = solveIDEProblemPll(Problem, HA.getICFG());
    }
  } else {
    llvm::errs() << "error: file does not contain a 'main' function!\n";
  }
  return 0;
}
