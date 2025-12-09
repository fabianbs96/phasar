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
    llvm::errs() << "myphasartool\n"
                    "A small PhASAR-based example program\n\n"
                    "Usage: myphasartool <LLVM IR file>\n";
    return 1;
  }

  std::vector EntryPoints = {"main"s};

  HelperAnalyses HA(Argv[1], EntryPoints);
  if (!HA.getProjectIRDB().isValid()) {
    return 1;
  }

  if (const auto *F = HA.getProjectIRDB().getFunctionDefinition("main")) {
    // print type hierarchy
    HA.getTypeHierarchy().print();
    // print points-to information
    HA.getAliasInfo().print();
    // print inter-procedural control-flow graph
    HA.getICFG().print();

    // IFDS template parametrization test
    llvm::outs() << "Testing IFDS:\n";

    auto DefaultConfig = LLVMTaintConfig(HA.getProjectIRDB());

    auto TaintProblem = createAnalysisProblem<IFDSTaintAnalysis>(
        HA, &DefaultConfig, EntryPoints);
    // IFDSSolver S = IFDSSolver<LLVMIFDSAnalysisDomainDefault,
    //                           SmallArraySet<const llvm::Value *>>(
    //     TaintProblem, &HA.getICFG());
    IFDSSolver S = IFDSSolver<LLVMIFDSAnalysisDomainDefault,
                              boost::container::flat_set<const llvm::Value *>>(
        TaintProblem, &HA.getICFG());
    auto IFDSResults = S.solve();
    IFDSResults.dumpResults(HA.getICFG());
  } else {
    llvm::errs() << "error: file does not contain a 'main' function!\n";
  }
  return 0;
}
