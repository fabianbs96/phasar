#include "phasar/DataFlow/IfdsIde/Solver/IFDSSolver.h"
#include "phasar/DataFlow/IfdsIde/Solver/IterativeIDESolver.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/DataFlow/IfdsIde/Problems/IFDSTaintAnalysis.h"
#include "phasar/PhasarLLVM/HelperAnalyses.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/SimpleAnalysisConstructor.h"
#include "phasar/PhasarLLVM/TaintConfig/LLVMTaintConfig.h"
#include "phasar/Utils/ChronoUtils.h"
#include "phasar/Utils/Timer.h"

using namespace psr;

int main(int Argc, const char **Argv) {
  using namespace std::string_literals;

  if (Argc < 2) {
    llvm::errs() << "myphasartool\n"
                    "A small PhASAR-based example program\n\n"
                    "Usage: myphasartool <LLVM IR file>\n";
    return 1;
  }

  std::vector EntryPoints = {"main"s};

  HelperAnalyses HA(Argv[1], EntryPoints,
                    {
                        .CGTy = CallGraphAnalysisType::RTA,
                    });
  if (!HA.getProjectIRDB().isValid()) {
    return 1;
  }

  if (const auto *F = HA.getProjectIRDB().getFunctionDefinition("main")) {
    // IFDS template parametrization test

    auto DefaultConfig = LLVMTaintConfig(HA.getProjectIRDB());

    auto TaintProblem = createAnalysisProblem<IFDSTaintAnalysis>(
        HA, &DefaultConfig, EntryPoints);

    // TODO: InterativeTypeSolver austesten

    IterativeIDESolver S(&TaintProblem, &HA.getICFG());

    Timer TimeSolve = Timer([](psr::hms ElapsedTime) {
      llvm::errs() << "IFDSResults ElapsedTime: " << ElapsedTime << "\n";
    });

    llvm::errs() << "Testing IFDS:\n";
    S.solve();
  } else {
    llvm::errs() << "error: file does not contain a 'main' function!\n";
  }
  return 0;
}
