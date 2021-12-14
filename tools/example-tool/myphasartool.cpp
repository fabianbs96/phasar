/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <chrono>
#include <fstream>
#include <iostream>

#include "boost/filesystem/operations.hpp"

#include "phasar/DB/ProjectIRDB.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/IDELinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Problems/IFDSLinearConstantAnalysis.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Solver/IDESolver.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/Solver/IFDSSolver.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToSet.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h"
#include "phasar/Utils/Logger.h"

#include "llvm/Support/GraphWriter.h"
namespace llvm {
class Value;
} // namespace llvm

using namespace psr;

int main(int Argc, const char **Argv) {
  initializeLogger(false);
  if (Argc < 2 || !boost::filesystem::exists(Argv[1]) ||
      boost::filesystem::is_directory(Argv[1])) {
    std::cerr << "myphasartool\n"
                 "A small PhASAR-based example program\n\n"
                 "Usage: myphasartool <LLVM IR file>\n";
    return 1;
  }
  ProjectIRDB DB({Argv[1]});
  if (const auto *F = DB.getFunctionDefinition("main")) {
    LLVMTypeHierarchy H(DB);

    LLVMPointsToSet P(DB);
    LLVMBasedICFG I(DB, CallGraphAnalysisType::OTF, {"main"}, &H, &P);

    {
      auto Start = std::chrono::high_resolution_clock::now();
      auto ICFGStr = I.exportICFGAsSourceCodeJsonString();
      auto End = std::chrono::high_resolution_clock::now();

      llvm::outs() << "New export: " << (End - Start).count() << '\n';
      llvm::outs() << "> size/capacity: " << ICFGStr.size() << '/'
                   << ICFGStr.capacity() << '\n';
      // llvm::outs() << ICFGStr << '\n';
    }

    {
      auto Start = std::chrono::high_resolution_clock::now();
      auto ICFGStr = I.exportICFGAsSourceCodeDotString();
      auto End = std::chrono::high_resolution_clock::now();

      llvm::outs() << "Dot export: " << (End - Start).count() << '\n';
      llvm::outs() << "> size/capacity: " << ICFGStr.size() << '/'
                   << ICFGStr.capacity() << '\n';
      // llvm::outs() << ICFGStr << '\n';
    }

    {
      auto Start = std::chrono::high_resolution_clock::now();
      auto ICFGJson = I.exportICFGAsSourceCodeJson();
      auto Mid = std::chrono::high_resolution_clock::now();
      auto ICFGStr = ICFGJson.dump();
      auto End = std::chrono::high_resolution_clock::now();

      llvm::outs() << "Old export: " << (End - Start).count() << '\n';
      llvm::outs() << "> To JSON took: " << (Mid - Start).count() << '\n';
      llvm::outs() << "> To Str took: " << (End - Mid).count() << '\n';
      llvm::outs() << "> size/capacity: " << ICFGStr.size() << '/'
                   << ICFGStr.capacity() << '\n';
    }
  } else {
    std::cerr << "error: file does not contain a 'main' function!\n";
  }
  return 0;
}
