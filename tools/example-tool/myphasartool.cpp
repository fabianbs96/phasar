/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <chrono>

#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"

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
  initializeLogger(false);
  if (Argc < 2 || !boost::filesystem::exists(Argv[1]) ||
      boost::filesystem::is_directory(Argv[1])) {
    llvm::errs() << "myphasartool\n"
                    "A small PhASAR-based example program\n\n"
                    "Usage: myphasartool <LLVM IR file>\n";
    return 1;
  }
  ProjectIRDB DB({Argv[1]});
  if (const auto *F = DB.getFunctionDefinition("main")) {
    LLVMTypeHierarchy H(DB);

    LLVMPointsToSet P(DB);
    LLVMBasedICFG I(DB, CallGraphAnalysisType::OTF, {"main"}, &H, &P);

    llvm::errs() << "ICFG constructed\n";

    {
      auto Start = std::chrono::high_resolution_clock::now();
      auto ICFGStr = I.exportICFGAsSourceCodeJsonString();
      auto End = std::chrono::high_resolution_clock::now();

      llvm::outs() << "New export: " << Elapsed{(End - Start).count()} << '\n';
      llvm::outs() << "> size/capacity: " << ICFGStr.size() << '/'
                   << ICFGStr.capacity() << '('
                   << llvm::format("%.3f", float(ICFGStr.size()) /
                                               float(ICFGStr.capacity()))
                   << ")\n";
      // llvm::outs() << ICFGStr << '\n';
    }

    // {
    //   auto Start = std::chrono::high_resolution_clock::now();
    //   auto ICFGStr = I.exportICFGAsSourceCodeJsonString2();
    //   auto End = std::chrono::high_resolution_clock::now();

    //   llvm::outs() << "New export2: "
    //                << llvm::formatv("{0:N}", (End - Start).count()) << '\n';
    //   llvm::outs() << "> size/capacity: " << ICFGStr.size() << '/'
    //                << ICFGStr.capacity() << '('
    //                << llvm::format("%.3f", float(ICFGStr.size()) /
    //                                            float(ICFGStr.capacity()))
    //                << ")\n";
    //   // llvm::outs() << ICFGStr << '\n';
    // }

    {
      auto Start = std::chrono::high_resolution_clock::now();
      auto ICFGStr = I.exportICFGAsSourceCodeDotString();
      auto End = std::chrono::high_resolution_clock::now();

      llvm::outs() << "Dot export: " << Elapsed{(End - Start).count()} << '\n';
      llvm::outs() << "> size/capacity: " << ICFGStr.size() << '/'
                   << ICFGStr.capacity() << '('
                   << llvm::format("%.3f", float(ICFGStr.size()) /
                                               float(ICFGStr.capacity()))
                   << ")\n";
      // llvm::outs() << ICFGStr << '\n';
    }

    {
      auto Start = std::chrono::high_resolution_clock::now();
      auto ICFGJson = I.exportICFGAsSourceCodeJson();
      llvm::errs() << "> After exportICFGAsSourceCodeJson()\n";
      auto Mid = std::chrono::high_resolution_clock::now();
      auto ICFGStr = ICFGJson.dump();
      llvm::errs() << "> After dump()\n";
      auto End = std::chrono::high_resolution_clock::now();

      llvm::outs() << "Old export:\t" << Elapsed{(End - Start).count()} << '\n';
      llvm::outs() << "> To JSON took:\t" << Elapsed{(Mid - Start).count()}
                   << '\n';
      llvm::outs() << "> To Str took:\t" << Elapsed{(End - Mid).count()}
                   << '\n';
      llvm::outs() << "> size/capacity: " << ICFGStr.size() << '/'
                   << ICFGStr.capacity() << '('
                   << llvm::format("%.3f", float(ICFGStr.size()) /
                                               float(ICFGStr.capacity()))
                   << ")\n";
    }

  } else {
    llvm::errs() << "error: file does not contain a 'main' function!\n";
  }
  return 0;
}
