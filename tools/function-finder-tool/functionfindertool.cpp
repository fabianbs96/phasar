/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/HelperAnalyses.h"

#include "phasar.h"

#include <filesystem>
#include <string>

using namespace psr;

bool searchForFunction(llvm::StringRef RelevantFunctionName,
                       HelperAnalyses &HA);

int main(int Argc, const char **Argv) {
  using namespace std::string_literals;

  if (Argc != 3 || !std::filesystem::exists(Argv[2]) ||
      std::filesystem::is_directory(Argv[2])) {
    llvm::errs()
        << "functionfinder\n"
           "A small PhASAR-based tool to find out which files use a specific "
           "function\n\n"
           "Usage: functionfinder \"name of function\" <LLVM IR file>\n";
    return 1;
  }

  HelperAnalyses HA(Argv[2], EntryPoints);
  if (!HA.getProjectIRDB().isValid()) {
    return 1;
  }

  bool Result = searchForFunction(Argv[1], HA);

  if (Result) {
    llvm::outs() << "\nThis file contains the function of interest!\n";
  } else {
    llvm::outs() << "\nFunction not found!\n";
  }

  return 0;
}

bool searchForFunction(llvm::StringRef RelevantFunctionName,
                       HelperAnalyses &HA) {
  for (const auto &Curr : HA.getProjectIRDB().getAllFunctions()) {
    if (RelevantFunctionName.equals(Curr->getName())) {
      return true;
    }
  }

  return false;
}
