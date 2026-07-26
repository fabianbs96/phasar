/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/DataFlow/IfdsIde/Solver/IFDSSolver.h"
#include "phasar/PhasarLLVM/AnalysisPipeline.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IFDSTaintAnalysis.h"
#include "phasar/PhasarLLVM/SimpleAnalysisConstructor.h"
#include "phasar/PhasarLLVM/TaintConfig/LLVMTaintConfig.h"
#include "phasar/Pointer/AliasAnalysisType.h"
#include "phasar/Utils/Timer.h"

#include "phasar.h"

#include <filesystem>
#include <string>
#include <type_traits>

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

  {
    SimpleTimer Tm;
    HelperAnalyses HA(Argv[1], {"main"},
                      {
                          .PTATy = psr::AliasAnalysisType::UnionFind,
                          .CGTy = psr::CallGraphAnalysisType::VTA,
                      });
    if (!HA.getProjectIRDB().isValid()) {
      return 1;
    }

    LLVMTaintConfig TC(HA.getProjectIRDB());
    auto TA = createAnalysisProblem<IFDSTaintAnalysis>(HA, &TC);
    solveIFDSProblem(TA, HA.getICFG());

    llvm::outs() << "Classical Taint Analysis elapsed: " << Tm.elapsed()
                 << '\n';
  }

  {
    SimpleTimer Tm;
    auto Pipeline = defaultPipeline(Argv[1])
                        .with(TaintConfigStage{})
                        .with(DataflowAnalysisStage{},
                              std::type_identity<IFDSTaintAnalysis>{})
                        .shared();

    Pipeline.solve();

    llvm::outs() << "Pipeline Taint Analysis elapsed: " << Tm.elapsed() << '\n';
  }

  return 0;
}
